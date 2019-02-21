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
#include <led_status.h>


static led_status_pattern_t unpaired = { .n=2, .delay=(int[]){ 1000, 1000 } };
static led_status_pattern_t pairing = { .n=5, .delay=(int[]){ 100, 100, 100, 600 } };
static led_status_pattern_t normal_mode = { 2, (int[]){ 100, 9900 } };


static void wifi_init() {
    struct sdk_station_config wifi_config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASSWORD,
    };

    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&wifi_config);
    sdk_wifi_station_connect();
}

const int led_gpio = 2;
bool led_on = false;

void led_write(bool on) {
    gpio_write(led_gpio, on ? 0 : 1);
}


void led_identify_task(void *_args) {
    for (int i=0; i<3; i++) {
        for (int j=0; j<2; j++) {
            led_write(true);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            led_write(false);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

        vTaskDelay(250 / portTICK_PERIOD_MS);
    }

    led_write(led_on);

    vTaskDelete(NULL);
}

void led_identify(homekit_value_t _value) {
    printf("LED identify\n");
    xTaskCreate(led_identify_task, "LED identify", 128, NULL, 2, NULL);
}

homekit_value_t led_on_get() {
    return HOMEKIT_BOOL(led_on);
}

void led_on_set(homekit_value_t value) {
    if (value.format != homekit_format_bool) {
        printf("Invalid value format: %d\n", value.format);
        return;
    }

    led_on = value.bool_value;
    led_write(led_on);
}


homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_lightbulb, .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Sample LED"),
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "HaPK"),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "037A2BABF19D"),
            HOMEKIT_CHARACTERISTIC(MODEL, "MyLED"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, led_identify),
            NULL
        }),
        HOMEKIT_SERVICE(LIGHTBULB, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Sample LED"),
            HOMEKIT_CHARACTERISTIC(
                ON, false,
                .getter=led_on_get,
                .setter=led_on_set
            ),
            NULL
        }),
        NULL
    }),
    NULL
};


static led_status_t led_status;
static bool paired = false;

void on_event(homekit_event_t event) {
    if (event == HOMEKIT_EVENT_SERVER_INITIALIZED) {
        led_status_set(led_status, paired ? &normal_mode : &unpaired);
    }
    else if (event == HOMEKIT_EVENT_CLIENT_CONNECTED) {
        if (!paired)
            led_status_set(led_status, &pairing);
    }
    else if (event == HOMEKIT_EVENT_CLIENT_DISCONNECTED) {
        if (!paired)
            led_status_set(led_status, &unpaired);
    }
    else if (event == HOMEKIT_EVENT_PAIRING_ADDED || event == HOMEKIT_EVENT_PAIRING_REMOVED) {
        paired = homekit_is_paired();
        led_status_set(led_status, paired ? &normal_mode : &unpaired);
    }
}


homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111",
    .on_event = on_event,
};


void user_init(void) {
    uart_set_baud(0, 115200);

    wifi_init();

    paired = homekit_is_paired();
    led_status = led_status_init(led_gpio);

    homekit_server_init(&config);
}
