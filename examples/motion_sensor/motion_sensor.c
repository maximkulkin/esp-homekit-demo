#define DEVICE_MANUFACTURER "Unknown"
#define DEVICE_NAME "Motion-Sensor"
#define DEVICE_MODEL "esp8266"
#define DEVICE_SERIAL "12345678"
#define FW_VERSION "1.0"
#define MOTION_SENSOR_GPIO 4
#define LED_GPIO 2

#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <string.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>

homekit_characteristic_t name             = HOMEKIT_CHARACTERISTIC_(NAME, DEVICE_NAME);
homekit_characteristic_t manufacturer     = HOMEKIT_CHARACTERISTIC_(MANUFACTURER,  DEVICE_MANUFACTURER);
homekit_characteristic_t serial           = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, DEVICE_SERIAL);
homekit_characteristic_t model            = HOMEKIT_CHARACTERISTIC_(MODEL,         DEVICE_MODEL);
homekit_characteristic_t revision         = HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION,  FW_VERSION);
homekit_characteristic_t motion_detected  = HOMEKIT_CHARACTERISTIC_(MOTION_DETECTED, 0);

void led_write(bool on) {
    gpio_write(LED_GPIO, on ? 0 : 1);
}

void identify_task(void *_args) {
    // We identify the board by Flashing it's LED.
    for (int i=0; i<3; i++) {
        for (int j=0; j<2; j++) {
            led_write(true);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            led_write(false);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

        vTaskDelay(250 / portTICK_PERIOD_MS);
    }
    led_write(false);
    vTaskDelete(NULL);
}

void identify(homekit_value_t _value) {
    printf("identify\n\n");
    xTaskCreate(identify_task, "identify", 128, NULL, 2, NULL);
}

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_switch, .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
            &name,
            &manufacturer,
            &serial,
            &model,
            &revision,
            HOMEKIT_CHARACTERISTIC(IDENTIFY, identify),
            NULL
        }),
        HOMEKIT_SERVICE(MOTION_SENSOR, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Motion Sensor"),
            &motion_detected,
            NULL
        }),
        NULL
    }),
    NULL
};

void motion_sensor_callback(uint8_t gpio) {
    if (gpio == MOTION_SENSOR_GPIO){
        int new = 0;
        new = gpio_read(MOTION_SENSOR_GPIO);
        motion_detected.value = HOMEKIT_BOOL(new);
        homekit_characteristic_notify(&motion_detected, HOMEKIT_BOOL(new));
    }
    else {
        printf("Interrupt on %d", gpio);
    }
}

void gpio_init() {
    gpio_enable(MOTION_SENSOR_GPIO, GPIO_INPUT);
    gpio_set_pullup(MOTION_SENSOR_GPIO, false, false);
    gpio_set_interrupt(MOTION_SENSOR_GPIO, GPIO_INTTYPE_EDGE_ANY, motion_sensor_callback);
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
    create_accessory_name();
    wifi_config_init("motion-sensor", NULL, on_wifi_ready);
    gpio_init();
}
