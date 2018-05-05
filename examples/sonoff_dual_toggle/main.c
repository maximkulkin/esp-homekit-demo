/*
 * Example of using esp-homekit library to control
 * a simple $5 Sonoff Dual R2 using HomeKit.
 * The esp-wifi-config library is also used in this
 * example. This means you don't have to specify
 * your network's SSID and password before building.
 *
 * In order to flash the sonoff dual you will have to
 * have a 3,3v (logic level) FTDI adapter.
 *
 * To flash this example connect 3,3v, TX, RX, GND
 * in this order, beginning in the (square) pin header
 * at the end of the board.
 *
 * Note unlike Sonoff Basic the physical button on the
 * dual does NOT connect to GPIO0 used to flash board!
 *
 * Instead short ground and "Button 0" on the side
 * header and connect the FTDI adapter to your computer.
 * The sonoff is now in flash mode and you can flash
 * the custom firmware.
 *
 * WARNING: Do not connect the sonoff to AC while it's
 * connected to the FTDI adapter! This may fry your
 * computer and sonoff.
 *
 * Changes:
 * Modified example Sonoff_basic_toggle to address two channels
 * Implemented as two linked homekit characteristics (2x switch)
 * Inabled push-button (GPIO10) as input and use to change state
 * implemented short/medium/long button press events to
 * toggle relay1, toggle relay2 and reset wifi respectivly
 * implemented toggle on "Button 0" (GPIO0) and "Button 1" (GPIO9)
 * Note: not long term solution as if "button 0" toggle is grounded
 * during restart board will enter flash mode. Solution would be
 * to swap connections of GPIO0 and GPIO10 resulting in push-button
 * being GPIO0 and "Button 0" being GPIO 10. Possible via breaking
 * circuit tracks and soldering new track.
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
#include "toggle.h"

// The GPIO pin that is connected to relay#1 on the Sonoff Dual.
const int relay_gpio_1 = 12;
// The GPIO pin that is connected to relay#2 on the Sonoff Dual.
const int relay_gpio_2 = 5;

// The GPIO pin that is connected to the LED on the Sonoff Dual.
const int led_gpio = 13;

// The GPIO pin that is connected to the button on the Sonoff Dual.
const int button_gpio = 10;

// The GPIO pin that is connected to the header on the Sonoff Dual (external switch).
const int toggle_gpio_1 = 0;
const int toggle_gpio_2 = 9;

void switch_on_1_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context);
void switch_on_2_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context);

void button_callback(uint8_t gpio, button_event_t event);

void toggle_callback_1(uint8_t gpio);
void toggle_callback_2(uint8_t gpio);


void relay_write_1(bool on) {
    gpio_write(relay_gpio_1, on ? 1 : 0);
}

void relay_write_2(bool on) {
    gpio_write(relay_gpio_2, on ? 1 : 0);
}

void led_write(bool on) {
    gpio_write(led_gpio, on ? 0 : 1);
}


void reset_configuration_task() {
    //Flash LED first before we start the reset
    for (int i=0; i<3; i++) {
        led_write(true);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        led_write(false);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    printf("Resetting Wifi   due to button long   at GPIO %2d\n", button_gpio);
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

homekit_characteristic_t switch_on_1 = HOMEKIT_CHARACTERISTIC_(
    ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch_on_1_callback)
);

homekit_characteristic_t switch_on_2 = HOMEKIT_CHARACTERISTIC_(
    ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch_on_2_callback)
);



void gpio_init() {
    gpio_enable(led_gpio, GPIO_OUTPUT);
    led_write(false);
    
    gpio_enable(relay_gpio_1, GPIO_OUTPUT);
    relay_write_2(switch_on_1.value.bool_value);
    
    gpio_enable(relay_gpio_2, GPIO_OUTPUT);
    relay_write_2(switch_on_2.value.bool_value);
    
    gpio_enable(button_gpio,   GPIO_INPUT);
    gpio_enable(toggle_gpio_1, GPIO_INPUT);
    gpio_enable(toggle_gpio_2, GPIO_INPUT);
}

void switch_on_1_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context) {
    relay_write_1(switch_on_1.value.bool_value);
}

void switch_on_2_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context) {
    relay_write_2(switch_on_2.value.bool_value);
}


void button_callback(uint8_t gpio, button_event_t event) {
    switch (event) {
        case button_event_single_press:
            printf("Toggling relay 1 due to button short  at GPIO %2d\n", gpio);
            switch_on_1.value.bool_value = !switch_on_1.value.bool_value;
            relay_write_1(switch_on_1.value.bool_value);
            homekit_characteristic_notify(&switch_on_1, switch_on_1.value);
            break;
        case button_event_medium_press:
            printf("Toggling relay 2 due to button medium at GPIO %2d\n", gpio);
            switch_on_2.value.bool_value = !switch_on_2.value.bool_value;
            relay_write_2(switch_on_2.value.bool_value);
            homekit_characteristic_notify(&switch_on_2, switch_on_2.value);
            break;
        case button_event_long_press:
            reset_configuration();
            break;
        default:
            printf("Unknown button event: %d\n", event);
    }
}

void toggle_callback_1(uint8_t gpio) {
    printf("Toggling relay 1 due to switch at GPIO %2d\n", gpio);
    switch_on_1.value.bool_value = !switch_on_1.value.bool_value;
    relay_write_1(switch_on_1.value.bool_value);
    homekit_characteristic_notify(&switch_on_1, switch_on_1.value);
}

void toggle_callback_2(uint8_t gpio) {
    printf("Toggling relay 2 due to switch at GPIO %2d\n", gpio);
    switch_on_2.value.bool_value = !switch_on_2.value.bool_value;
    relay_write_2(switch_on_2.value.bool_value);
    homekit_characteristic_notify(&switch_on_2, switch_on_2.value);
}


void switch_identify_task(void *_args) {
    // We identify the Sonoff by Flashing it's LED.
    for (int i=0; i<3; i++) {
        for (int j=0; j<2; j++) {
            led_write(true);
            vTaskDelay(200 / portTICK_PERIOD_MS);
            led_write(false);
            vTaskDelay(200 / portTICK_PERIOD_MS);
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    led_write(false);
    vTaskDelete(NULL);
}

void switch_identify(homekit_value_t _value) {
    printf("Switch identify\n");
    xTaskCreate(switch_identify_task, "Switch identify", 128, NULL, 2, NULL);
}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Sonoff Dual");

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_switch, .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(IDENTIFY, switch_identify),
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "iTEAD"),
            HOMEKIT_CHARACTERISTIC(MODEL, "Sonoff Dual"),
            &name,
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "mps"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1"),
            HOMEKIT_CHARACTERISTIC(HARDWARE_REVISION, "R2"),
            NULL
        }),
        HOMEKIT_SERVICE(SWITCH, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Dual 1"),
            &switch_on_1,
            NULL
        }),
        HOMEKIT_SERVICE(SWITCH, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Dual 2"),
            &switch_on_2,
            NULL
        }),
        NULL
    }),
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "190-11-978"    //changed tobe valid
//    .password = "111-11-111"    //default easy
};

void on_wifi_ready() {
    homekit_server_init(&config);
}

void create_accessory_name() {
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);
    
    int name_len = snprintf(NULL, 0, "Sonoff Dual %02X:%02X:%02X",
            macaddr[3], macaddr[4], macaddr[5]);
    char *name_value = malloc(name_len+1);
    snprintf(name_value, name_len+1, "Sonoff Dual %02X:%02X:%02X",
            macaddr[3], macaddr[4], macaddr[5]);
    
    name.value = HOMEKIT_STRING(name_value);
}


void user_init(void) {
    uart_set_baud(0, 115200);
    create_accessory_name();
    wifi_config_init("Sonoff Dual", NULL, on_wifi_ready);
    gpio_init();

// 4000 = 4s long press => 4000/4 = 1s medium press
    if (button_create(button_gpio, 0, 4000, button_callback)) {
        printf("Failed to initialize button\n");
    }
    
    if (toggle_create(toggle_gpio_1, toggle_callback_1)) {
        printf("Failed to initialize toggle 1 \n");
    }

    if (toggle_create(toggle_gpio_2, toggle_callback_2)) {
        printf("Failed to initialize toggle 2 \n");
    }
}
