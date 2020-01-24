/*
 * Occupancy sensor for HomeKit based on HC-SR501.
 */

#include <stdio.h>
#include <esp/uart.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>

#include <toggle.h>
#include <wifi_config.h>


#ifndef SENSOR_PIN
#error SENSOR_PIN is not specified
#endif


void occupancy_identify(homekit_value_t _value) {
    printf("Occupancy identify\n");
}


homekit_characteristic_t occupancy_detected = HOMEKIT_CHARACTERISTIC_(OCCUPANCY_DETECTED, 0);


void sensor_callback(bool high, void *context) {
    occupancy_detected.value = HOMEKIT_UINT8(high ? 1 : 0);
    homekit_characteristic_notify(&occupancy_detected, occupancy_detected.value);
}


homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_sensor, .services=(homekit_service_t*[]) {
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Occupancy Sensor"),
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "HaPK"),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "1"),
            HOMEKIT_CHARACTERISTIC(MODEL, "Occupancy Sensor"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, occupancy_identify),
            NULL
        }),
        HOMEKIT_SERVICE(OCCUPANCY_SENSOR, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Occupancy Sensor"),
            &occupancy_detected,
            NULL
        }),
        NULL
    }),
    NULL
};


static bool homekit_initialized = false;
static homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111"
};


void on_wifi_config_event(wifi_config_event_t event) {
    if (event == WIFI_CONFIG_CONNECTED) {
        if (!homekit_initialized) {
            homekit_server_init(&config);
            homekit_initialized = true;
        }
    }
}


void user_init(void) {
    uart_set_baud(0, 115200);

    wifi_config_init2("occupancy-sensor", NULL, on_wifi_config_event);

    if (toggle_create(SENSOR_PIN, sensor_callback, NULL)) {
        printf("Failed to initialize sensor\n");
    }
}
