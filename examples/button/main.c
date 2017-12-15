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
#include "button.h"


#ifndef BUTTON_PIN
#error BUTTON_PIN is not specified
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


void button_identify(homekit_value_t _value) {
    printf("Button identify\n");
}


homekit_characteristic_t button_event = {
    .type = HOMEKIT_CHARACTERISTIC_PROGRAMMABLE_SWITCH_EVENT,
    .format = homekit_format_uint8,
    .permissions = homekit_permissions_paired_read
                 | homekit_permissions_notify,
    .min_value = (float[]) {0},
    .max_value = (float[]) {2},
    .min_step = (float[]) {1},
    .value.is_null = true,
    .valid_values = {
        .count = 3,
        .values = (uint8_t[]) {0, 1, 2},
    }
};


void button_callback(uint8_t gpio, button_event_t event) {
    switch (event) {
        case button_event_single_press:
            printf("single press\n");
            homekit_characteristic_notify(&button_event, HOMEKIT_UINT8(0));
            break;
        case button_event_double_press:
            printf("double press\n");
            homekit_characteristic_notify(&button_event, HOMEKIT_UINT8(1));
            break;
        case button_event_long_press:
            printf("long press\n");
            homekit_characteristic_notify(&button_event, HOMEKIT_UINT8(2));
            break;
        default:
            printf("unknown button event: %d\n", event);
    }
}


homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(
        .id=1,
        .category=homekit_accessory_category_programmable_switch,
        .services=(homekit_service_t*[]) {
            HOMEKIT_SERVICE(
                HOMEKIT_SERVICE_ACCESSORY_INFORMATION,
                .characteristics=(homekit_characteristic_t*[]) {
                    HOMEKIT_DECLARE_CHARACTERISTIC_NAME("Button"),
                    HOMEKIT_DECLARE_CHARACTERISTIC_MANUFACTURER("HaPK"),
                    HOMEKIT_DECLARE_CHARACTERISTIC_SERIAL_NUMBER("0012345"),
                    HOMEKIT_DECLARE_CHARACTERISTIC_MODEL("MyButton"),
                    HOMEKIT_DECLARE_CHARACTERISTIC_FIRMWARE_REVISION("0.1"),
                    HOMEKIT_DECLARE_CHARACTERISTIC_IDENTIFY(button_identify),
                    NULL
                },
            ),
            HOMEKIT_SERVICE(
                HOMEKIT_SERVICE_STATELESS_PROGRAMMABLE_SWITCH,
                .primary=true,
                .characteristics=(homekit_characteristic_t*[]) {
                    HOMEKIT_DECLARE_CHARACTERISTIC_NAME("Button"),
                    &button_event,
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
    uart_set_baud(0, 115200);

    wifi_init();
    if (button_create(BUTTON_PIN, button_callback)) {
        printf("Failed to initialize button\n");
    }
    homekit_server_init(&config);
}

