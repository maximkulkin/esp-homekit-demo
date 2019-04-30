#include <stdio.h>
#include <string.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>

#include "wifi.h"


#define MAX_SERVICES 20
#define MAX_CHARACTERISTICS 10


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

const uint8_t relay_gpios[] = {
  12, 5, 14, 13
};
const size_t relay_count = sizeof(relay_gpios) / sizeof(*relay_gpios);


void relay_write(int relay, bool on) {
    printf("Relay %d %s\n", relay, on ? "ON" : "OFF");
    gpio_write(relay, on ? 1 : 0);
}

void led_write(bool on) {
    gpio_write(led_gpio, on ? 0 : 1);
}

void gpio_init() {
    gpio_enable(led_gpio, GPIO_OUTPUT);
    led_write(false);

    for (int i=0; i < relay_count; i++) {
        gpio_enable(relay_gpios[i], GPIO_OUTPUT);
        relay_write(relay_gpios[i], true);
    }
}

void lamp_identify_task(void *_args) {
    relay_write(relay_gpios[0], true);

    for (int i=0; i<3; i++) {
        for (int j=0; j<2; j++) {
            relay_write(relay_gpios[0], true);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            relay_write(relay_gpios[0], false);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

        vTaskDelay(250 / portTICK_PERIOD_MS);
    }

    relay_write(relay_gpios[0], true);

    vTaskDelete(NULL);
}

void lamp_identify(homekit_value_t _value) {
    printf("Lamp identify\n");
    xTaskCreate(lamp_identify_task, "Lamp identify", 128, NULL, 2, NULL);
}

void relay_callback(homekit_characteristic_t *ch, homekit_value_t value, void *context) {
    uint8_t *gpio = context;
    relay_write(*gpio, value.bool_value);
}


homekit_accessory_t *accessories[2];

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111"
};

void on_wifi_ready() {
    homekit_server_init(&config);
}


void* memdup(void* data, size_t data_size) {
    void *result = malloc(data_size);
    memcpy(result, data, data_size);
    return result;
}

#define MEMDUP_RANGE(start, end) \
    memdup(start, ((uint8_t*)end) - ((uint8_t*)start))

#define NEW_HOMEKIT_ACCESSORY(...) \
    memdup(HOMEKIT_ACCESSORY(__VA_ARGS__), sizeof(homekit_accessory_t))

#define NEW_HOMEKIT_SERVICE(name, ...) \
    memdup(HOMEKIT_SERVICE(name, ## __VA_ARGS__), sizeof(homekit_service_t))

#define NEW_HOMEKIT_CHARACTERISTIC(name, ...) \
    memdup(HOMEKIT_CHARACTERISTIC(name, ## __VA_ARGS__), sizeof(homekit_characteristic_t))

#define NEW_HOMEKIT_CHARACTERISTIC_CALLBACK(...) \
    memdup(HOMEKIT_CHARACTERISTIC_CALLBACK(__VA_ARGS__), \
           sizeof(homekit_characteristic_change_callback_t))


void init_accessory() {
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);

    int name_len = snprintf(NULL, 0, "Relays-%02X%02X%02X",
                            macaddr[3], macaddr[4], macaddr[5]);
    char *name_value = malloc(name_len+1);
    snprintf(name_value, name_len+1, "Relays-%02X%02X%02X",
             macaddr[3], macaddr[4], macaddr[5]);

    homekit_service_t* services[MAX_SERVICES + 1];
    homekit_characteristic_t* characteristics[MAX_CHARACTERISTICS + 1];

    homekit_service_t** s = services;
    homekit_characteristic_t** c;

    c = characteristics;
    *(c++) = NEW_HOMEKIT_CHARACTERISTIC(NAME, name_value);
    *(c++) = NEW_HOMEKIT_CHARACTERISTIC(MANUFACTURER, "HaPK");
    *(c++) = NEW_HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "0");
    *(c++) = NEW_HOMEKIT_CHARACTERISTIC(MODEL, "Relays");
    *(c++) = NEW_HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1");
    *(c++) = NEW_HOMEKIT_CHARACTERISTIC(IDENTIFY, lamp_identify);
    *(c++) = NULL;

    *(s++) = NEW_HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=MEMDUP_RANGE(characteristics, c));

    for (int i=0; i < relay_count; i++) {
        int relay_name_len = snprintf(NULL, 0, "Relay %d", i + 1);
        char *relay_name_value = malloc(name_len+1);
        snprintf(relay_name_value, relay_name_len+1, "Relay %d", i + 1);

        c = characteristics;
        *(c++) = NEW_HOMEKIT_CHARACTERISTIC(NAME, relay_name_value);
        *(c++) = NEW_HOMEKIT_CHARACTERISTIC(
            ON, true,
            .callback=NEW_HOMEKIT_CHARACTERISTIC_CALLBACK(relay_callback, .context=(void*)&relay_gpios[i]),
        );
        *(c++) = NULL;

        *(s++) = NEW_HOMEKIT_SERVICE(LIGHTBULB, .characteristics=MEMDUP_RANGE(characteristics, c));
    }

    *(s++) = NULL;

    accessories[0] = NEW_HOMEKIT_ACCESSORY(
        .category=homekit_accessory_category_lightbulb,
        .services=MEMDUP_RANGE(services, s),
    );
    accessories[1] = NULL;
}

void user_init(void) {
    uart_set_baud(0, 115200);

    init_accessory();

    gpio_init();

    wifi_init();
    on_wifi_ready();
}
