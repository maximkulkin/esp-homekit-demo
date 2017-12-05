#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>

#include <homekit/homekit.h>
#include <homekit/types.h>
#include <homekit/characteristics.h>
#include "wifi.h"


static void wifi_init() {
    struct sdk_station_config wifi_config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASSWORD,
    };

    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&wifi_config);
    sdk_wifi_station_connect();
}

const int temperature_sensor_gpio = 2;

void temperature_sensor_identify(bool _value) {
    printf("Temperature sensor identify\n");
}

homekit_characteristic_t temperature = {
    .type = HOMEKIT_CHARACTERISTIC_CURRENT_TEMPERATURE,
    .format = homekit_format_float,
    .unit = homekit_unit_celsius,
    .permissions = homekit_permissions_paired_read
                 | homekit_permissions_notify,
    .min_value = (float[]) {0},
    .max_value = (float[]) {100},
    .min_step = (float[]) {0.1},
};

void temperature_sensor_task(void *_args) {
    while (1) {
        temperature.float_value = hwrand() % 100;
        homekit_characteristic_notify(&temperature);

        vTaskDelay(500);
    }
}

void temperature_sensor_init() {
    xTaskCreate(temperature_sensor_task, "Temperatore Sensor", 128, NULL, 2, NULL);
}


homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_thermostat, .services={
        HOMEKIT_SERVICE(HOMEKIT_SERVICE_ACCESSORY_INFORMATION, .characteristics={
            HOMEKIT_DECLARE_CHARACTERISTIC_NAME("Temperature Sensor"),
            HOMEKIT_DECLARE_CHARACTERISTIC_MANUFACTURER("HaPK"),
            HOMEKIT_DECLARE_CHARACTERISTIC_SERIAL_NUMBER("037A2BABF19D"),
            HOMEKIT_DECLARE_CHARACTERISTIC_MODEL("MyTemperatureSensor"),
            HOMEKIT_DECLARE_CHARACTERISTIC_FIRMWARE_REVISION("0.1"),
            HOMEKIT_DECLARE_CHARACTERISTIC_IDENTIFY(temperature_sensor_identify),
        }),
        HOMEKIT_SERVICE(HOMEKIT_SERVICE_TEMPERATURE_SENSOR, .primary=true, .characteristics={
            HOMEKIT_DECLARE_CHARACTERISTIC_NAME("Temperature Sensor"),
            &temperature
        }),
    }),
    NULL
};


void user_init(void) {
    uart_set_baud(0, 115200);

    wifi_init();
    temperature_sensor_init();
    homekit_server_init(accessories);
}

