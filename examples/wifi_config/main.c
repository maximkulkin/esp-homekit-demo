/*
 * Example of using esp-wifi-config library to join
 * accessories to WiFi networks.
 *
 * When accessory starts without WiFi config or
 * configure WiFi network is not available, it creates
 * it's own WiFi AP "my-accessory-XXXXXX" (where XXXXXX
 * is last 3 digits of accessory's mac address in HEX).
 * If you join that network, you will be presented with
 * a page to choose WiFi network to connect accessory to.
 *
 * After successful connection accessory shuts down AP.
 *
 */

#include <stdio.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>


const int led_gpio = 2;

void led_write(bool on) {
    gpio_write(led_gpio, on ? 0 : 1);
}


void led_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context);

homekit_characteristic_t led_on = HOMEKIT_CHARACTERISTIC_(
    ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(led_on_callback)
);

void led_init() {
    gpio_enable(led_gpio, GPIO_OUTPUT);
    led_write(led_on.value.bool_value);
}

void led_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context) {
    led_write(led_on.value.bool_value);
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

    led_write(led_on.value.bool_value);

    vTaskDelete(NULL);
}

void led_identify(homekit_value_t _value) {
    printf("LED identify\n");
    xTaskCreate(led_identify_task, "LED identify", 128, NULL, 2, NULL);
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
            &led_on,
            NULL
        }),
        NULL
    }),
    NULL
};

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111"
};

void on_wifi_ready() {
    homekit_server_init(&config);
}

void user_init(void) {
    uart_set_baud(0, 115200);

    wifi_config_init("my-accessory", NULL, on_wifi_ready);
    led_init();
}
