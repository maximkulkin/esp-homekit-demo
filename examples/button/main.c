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
#include <button.h>


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


homekit_characteristic_t button_event = HOMEKIT_CHARACTERISTIC_(PROGRAMMABLE_SWITCH_EVENT, 0);


void button_callback(button_event_t event, void *context) {
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
                ACCESSORY_INFORMATION,
                .characteristics=(homekit_characteristic_t*[]) {
                    HOMEKIT_CHARACTERISTIC(NAME, "Button"),
                    HOMEKIT_CHARACTERISTIC(MANUFACTURER, "HaPK"),
                    HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "0012345"),
                    HOMEKIT_CHARACTERISTIC(MODEL, "MyButton"),
                    HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1"),
                    HOMEKIT_CHARACTERISTIC(IDENTIFY, button_identify),
                    NULL
                },
            ),
            HOMEKIT_SERVICE(
                STATELESS_PROGRAMMABLE_SWITCH,
                .primary=true,
                .characteristics=(homekit_characteristic_t*[]) {
                    HOMEKIT_CHARACTERISTIC(NAME, "Button"),
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
    button_config_t button_config = BUTTON_CONFIG(
        button_active_low, 
        .max_repeat_presses=2,
        .long_press_time=1000,
    );
    if (button_create(BUTTON_PIN, button_config, button_callback, NULL)) {
        printf("Failed to initialize button\n");
    }
    homekit_server_init(&config);
}

