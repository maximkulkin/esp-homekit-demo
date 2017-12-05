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

const int led_gpio = 2;
bool led_on = false;

void led_write(bool on) {
    gpio_write(led_gpio, on ? 0 : 1);
}

void led_init() {
    gpio_enable(led_gpio, GPIO_OUTPUT);
    led_write(led_on);
}

void led_identify_task(void *_args) {
    for (int i=0; i<3; i++) {
        led_write(true);
        vTaskDelay(500 / portTICK_PERIOD_MS);
        led_write(false);
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }

    led_write(led_on);

    vTaskDelete(NULL);
}

void led_identify(bool _value) {
    printf("LED identify\n");
    xTaskCreate(led_identify_task, "LED identify", 128, NULL, 2, NULL);
}

bool led_on_get() {
    return led_on;
}

void led_on_set(bool value) {
    led_on = value;
    led_write(led_on);
}


homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_lightbulb, .services={
        HOMEKIT_SERVICE(HOMEKIT_SERVICE_ACCESSORY_INFORMATION, .characteristics={
            HOMEKIT_DECLARE_CHARACTERISTIC_NAME("Sample LED"),
            HOMEKIT_DECLARE_CHARACTERISTIC_MANUFACTURER("HaPK"),
            HOMEKIT_DECLARE_CHARACTERISTIC_SERIAL_NUMBER("037A2BABF19D"),
            HOMEKIT_DECLARE_CHARACTERISTIC_MODEL("MyLED"),
            HOMEKIT_DECLARE_CHARACTERISTIC_FIRMWARE_REVISION("0.1"),
            HOMEKIT_DECLARE_CHARACTERISTIC_IDENTIFY(led_identify),
        }),
        HOMEKIT_SERVICE(HOMEKIT_SERVICE_LIGHTBULB, .primary=true, .characteristics={
            HOMEKIT_DECLARE_CHARACTERISTIC_NAME("Sample LED"),
            HOMEKIT_DECLARE_CHARACTERISTIC_ON(
                false,
                .getter=led_on_get,
                .setter=led_on_set
            ),
        }),
    }),
    NULL
};


void user_init(void) {
    uart_set_baud(0, 115200);

    wifi_init();
    led_init();
    homekit_server_init(accessories);
}
