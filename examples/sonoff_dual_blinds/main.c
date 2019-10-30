/*
 * Example of using Sonoff Dual R2 for controlling window blids
 * Also, it uses existing momentary wall switches connected to
 * GPIOs to control up and down commands
 */
#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>

#include "button.h"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define POSITION_OPEN 100
#define POSITION_CLOSED 0
#define POSITION_STATE_CLOSING 0
#define POSITION_STATE_OPENING 1
#define POSITION_STATE_STOPPED 2

// number of seconds the blinds take to move from fully open to fully closed position
#define SECONDS_FROM_CLOSED_TO_OPEN 15

TaskHandle_t updateStateTask;
homekit_characteristic_t current_position;
homekit_characteristic_t target_position;
homekit_characteristic_t position_state;
homekit_accessory_t *accessories[];



// The GPIO pin that is connected to the relay on the Sonoff Dual R2
const int relay0_gpio = 12;
const int relay1_gpio = 5;
// The GPIO pin that is connected to the LED on the Sonoff Dual R2
const int led_gpio = 13;
// The GPIO pin that is connected to the button on the Sonoff Dual R2
const int button_gpio = 10;
// The GPIO pin that is connected to the BUTTON1 header pin
const int button1_gpio = 9;
// The GPIO pin that is connected to the BUTTON0 header pin
const int button0_gpio = 0;

const int button_up = 9;
const int button_down = 0;
const int relay_up = 12;
const int relay_down = 5;

void target_position_changed();


void relay_write(int relay, bool on) {
    gpio_write(relay, on ? 1 : 0);
}

void led_write(bool on) {
    gpio_write(led_gpio, on ? 0 : 1);
}

void relays_write(int state) {
    switch (state){
	case POSITION_STATE_CLOSING:
	    gpio_write(relay_up, 0);
	    gpio_write(relay_down, 1);
	    break;
	case POSITION_STATE_OPENING:
	    gpio_write(relay_down, 0);
	    gpio_write(relay_up, 1);
	    break;
	default:
	    gpio_write(relay_down, 0);
	    gpio_write(relay_up, 0);
    }
}

void reset_configuration_task() {
    //Flash the LED first before we start the reset
    for (int i=0; i<3; i++) {
        led_write(true);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        led_write(false);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    printf("Resetting Wifi Config\n");

    wifi_config_reset();

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    printf("Resetting HomeKit Config\n");

    homekit_server_reset();

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    printf("Restarting\n");

    sdk_system_restart();

    vTaskDelete(NULL);
}

void reset_configuration() {
    printf("Resetting Sonoff configuration\n");
    xTaskCreate(reset_configuration_task, "Reset configuration", 256, NULL, 2, NULL);
}

void gpio_init() {
    gpio_enable(led_gpio, GPIO_OUTPUT);
    led_write(false);

    gpio_enable(button_up, GPIO_INPUT);
    gpio_enable(button_down, GPIO_INPUT);

    gpio_enable(relay0_gpio, GPIO_OUTPUT);
    gpio_enable(relay1_gpio, GPIO_OUTPUT);
    relay_write(relay0_gpio, false);
    relay_write(relay1_gpio, false);
}

void button_up_callback(uint8_t gpio_num, button_event_t event) {
    // up button pressed
    if (position_state.value.int_value != POSITION_STATE_STOPPED){ // if moving, stop
	target_position.value.int_value = current_position.value.int_value;
	target_position_changed();
    }else{
        switch (event) {
            case button_event_single_press:
	        target_position.value.int_value = POSITION_OPEN;
                target_position_changed();
                break;
            case button_event_long_press:
                //reset_configuration();
                break;
            default:
                printf("Unknown button event: %d\n", event);
        }
    }
}

void button_down_callback(uint8_t gpio_num, button_event_t event) {
    // down button pressed
    if (position_state.value.int_value != POSITION_STATE_STOPPED){ // if moving, stop
	target_position.value.int_value = current_position.value.int_value;
	target_position_changed();
    }else{
        switch (event) {
            case button_event_single_press:
	        target_position.value.int_value = POSITION_CLOSED;
                target_position_changed();
                break;
            case button_event_long_press:
                //reset_configuration();
                break;
            default:
                printf("Unknown button event: %d\n", event);
        }
    }
}

void update_state() {
    while (true) {
printf("update_state\n");
	relays_write(position_state.value.int_value);
        uint8_t position = current_position.value.int_value;
        int8_t direction = position_state.value.int_value == POSITION_STATE_OPENING ? 1 : -1;
        int16_t newPosition = position + direction;

        printf("position %u, target %u\n", newPosition, target_position.value.int_value);

        current_position.value.int_value = newPosition;
        homekit_characteristic_notify(&current_position, current_position.value);

        if (newPosition == target_position.value.int_value) {
            printf("reached destination %u\n", newPosition);
            position_state.value.int_value = POSITION_STATE_STOPPED;
	    relays_write(position_state.value.int_value);
            homekit_characteristic_notify(&position_state, position_state.value);
            vTaskSuspend(updateStateTask);
        }

        vTaskDelay(pdMS_TO_TICKS(SECONDS_FROM_CLOSED_TO_OPEN * 10));
    }
}

void update_state_init() {
    xTaskCreate(update_state, "UpdateState", 256, NULL, tskIDLE_PRIORITY, &updateStateTask);
    vTaskSuspend(updateStateTask);
}

void window_covering_identify(homekit_value_t _value) {
    printf("Curtain identify\n");
}

void on_update_target_position(homekit_characteristic_t *ch, homekit_value_t value, void *context);

homekit_characteristic_t current_position = {
    HOMEKIT_DECLARE_CHARACTERISTIC_CURRENT_POSITION(POSITION_CLOSED)
};

homekit_characteristic_t target_position = {
    HOMEKIT_DECLARE_CHARACTERISTIC_TARGET_POSITION(POSITION_CLOSED, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_update_target_position))
};

homekit_characteristic_t position_state = {
    HOMEKIT_DECLARE_CHARACTERISTIC_POSITION_STATE(POSITION_STATE_STOPPED)
};

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_window_covering, .services=(homekit_service_t*[]) {
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Window blind"),
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Sonoff"),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "001"),
            HOMEKIT_CHARACTERISTIC(MODEL, "Dual R2"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, window_covering_identify),
            NULL
        }),
        HOMEKIT_SERVICE(WINDOW_COVERING, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Window blind"),
            &current_position,
            &target_position,
            &position_state,
            NULL
        }),
        NULL
    }),
    NULL
};

void on_update_target_position(homekit_characteristic_t *ch, homekit_value_t value, void *context) {
    target_position_changed();
}

void target_position_changed(){
    printf("Update target position to: %u\n", target_position.value.int_value);

    if (target_position.value.int_value == current_position.value.int_value) {
        printf("Current position equal to target. Stopping.\n");
        position_state.value.int_value = POSITION_STATE_STOPPED;
	relays_write(position_state.value.int_value);
        homekit_characteristic_notify(&position_state, position_state.value);
        vTaskSuspend(updateStateTask);
    } else {
        position_state.value.int_value = target_position.value.int_value > current_position.value.int_value
            ? POSITION_STATE_OPENING
            : POSITION_STATE_CLOSING;

        homekit_characteristic_notify(&position_state, position_state.value);
        vTaskResume(updateStateTask);
    }
}

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111"
};

void on_wifi_ready() {
    homekit_server_init(&config);
}

void user_init(void) {
    uart_set_baud(0, 115200);

    gpio_init();

    wifi_config_init("blinds", NULL, on_wifi_ready);
    update_state_init();

    if (button_create(button_up, 0, 1000, button_up_callback)) {
        printf("Failed to initialize button up\n");
    }
    if (button_create(button_down, 0, 1000, button_down_callback)) {
        printf("Failed to initialize button down\n");
    }
}
