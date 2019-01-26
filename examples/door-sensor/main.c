#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include "wifi.h"
#include "contact_sensor.h"

#ifndef REED_PIN
#error REED_PIN is not specified
#endif


static void wifi_init() {
    struct sdk_station_config wifi_config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASSWORD,
    };

    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&wifi_config);
    sdk_wifi_station_connect();
}

/**
 * Called during pairing to let the user know which accessorty is being paired.
 **/
void door_identify(homekit_value_t _value) {
    printf("Door identifying\n");
    // The sensor cannot identify itself.
    // Nothing to do here.
}

/**
 * Returns the door sensor state as a homekit value.
 **/
homekit_value_t door_state_getter() {
    printf("Door state was requested (%s).\n", contact_sensor_state_get(REED_PIN) == CONTACT_OPEN ? "open" : "closed");
    return HOMEKIT_UINT8(contact_sensor_state_get(REED_PIN) == CONTACT_OPEN ? 1 : 0);
}

/**
 * The sensor characteristic as global variable.
 **/
homekit_characteristic_t door_open_characteristic = HOMEKIT_CHARACTERISTIC_(CONTACT_SENSOR_STATE, 0,
    .getter=door_state_getter,
    .setter=NULL,
    NULL
);

/**
 * Called (indirectly) from the interrupt handler to notify the client of a state change.
 **/
void contact_sensor_callback(uint8_t gpio, contact_sensor_state_t state) {
    switch (state) {
        case CONTACT_OPEN:
        case CONTACT_CLOSED:
            printf("Pushing contact sensor state '%s'.\n", state == CONTACT_OPEN ? "open" : "closed");
            homekit_characteristic_notify(&door_open_characteristic, door_state_getter());
            break;
        default:
            printf("Unknown contact sensor event: %d\n", state);
    }
}

/**
 * An array of the accessories (one) provided contining one service.
 **/
homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(
        .id=1,
        .category=homekit_accessory_category_sensor,
        .services=(homekit_service_t*[]) {
            HOMEKIT_SERVICE(
                ACCESSORY_INFORMATION,
                .characteristics=(homekit_characteristic_t*[]) {
                    HOMEKIT_CHARACTERISTIC(NAME, "Contact Sensor"),
                    HOMEKIT_CHARACTERISTIC(MANUFACTURER, "ObjP"),
                    HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "2012345"),
                    HOMEKIT_CHARACTERISTIC(MODEL, "DS1"),
                    HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1"),
                    HOMEKIT_CHARACTERISTIC(IDENTIFY, door_identify),
                    NULL
                },
            ),
            HOMEKIT_SERVICE(
                CONTACT_SENSOR,
                .primary=true,
                .characteristics=(homekit_characteristic_t*[]) {
                    HOMEKIT_CHARACTERISTIC(NAME, "Kontakt"),
                    &door_open_characteristic,
                    NULL
                },
            ),
            NULL
        },
    ),
    NULL
};


homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111"
};


void user_init(void) {
    uart_set_baud(0, 9600);

    wifi_init();
    printf("Using Sensor at GPIO%d.\n", REED_PIN);
    if (contact_sensor_create(REED_PIN, contact_sensor_callback)) {
        printf("Failed to initialize door\n");
    }
    homekit_server_init(&config);

    homekit_characteristic_notify(&door_open_characteristic, door_state_getter());
}

