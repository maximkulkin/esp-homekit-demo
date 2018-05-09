/*
 * Thermostat example
 *
 * Wiring is as follows (requires ESP-12 as it needs 4 GPIOs):
 *
 * DHT11 (temperature sensor)
 *
 *               -------------
 *              |GND       VCC|    (These go to control pins of relay)
 *              |15         13| --> Heater
 *              |2          12| --> Cooler
 *              |0          14| --> Fan
 *              |5          16|
 * DHT Data <-- |4       CH_PD|
 *              |RXD       ADC|
 *              |TXD      REST|
 *               -------------
 *              |   |-| |-| | |
 *              | __| |_| |_| |
 *               -------------
 *
 */
#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_system.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <etstimer.h>
#include <esplibs/libmain.h>
#include <FreeRTOS.h>
#include <task.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include "wifi.h"

#include <dht/dht.h>


#define LED_PIN 2
#define TEMPERATURE_SENSOR_PIN 4
#define FAN_PIN 14
#define COOLER_PIN 12
#define HEATER_PIN 13
#define TEMPERATURE_POLL_PERIOD 10000
#define HEATER_FAN_DELAY 30000
#define COOLER_FAN_DELAY 0


static void wifi_init() {
    struct sdk_station_config wifi_config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASSWORD,
    };

    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&wifi_config);
    sdk_wifi_station_connect();
}


void thermostat_identify(homekit_value_t _value) {
    printf("Thermostat identify\n");
}




ETSTimer fan_timer;


void heaterOn() {
    gpio_write(HEATER_PIN, false);
}


void heaterOff() {
    gpio_write(HEATER_PIN, true);
}


void coolerOn() {
    gpio_write(COOLER_PIN, false);
}


void coolerOff() {
    gpio_write(COOLER_PIN, true);
}


void fan_alarm(void *arg) {
    gpio_write(FAN_PIN, false);
}

void fanOn(uint16_t delay) {
    if (delay > 0) {
        sdk_os_timer_arm(&fan_timer, delay, false);
    } else {
        gpio_write(FAN_PIN, false);
    }
}


void fanOff() {
    sdk_os_timer_disarm(&fan_timer);
    gpio_write(FAN_PIN, true);
}


void update_state();


void on_update(homekit_characteristic_t *ch, homekit_value_t value, void *context) {
    update_state();
}


homekit_characteristic_t current_temperature = HOMEKIT_CHARACTERISTIC_(
    CURRENT_TEMPERATURE, 0
);
homekit_characteristic_t target_temperature  = HOMEKIT_CHARACTERISTIC_(
    TARGET_TEMPERATURE, 22, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_update)
);
homekit_characteristic_t units = HOMEKIT_CHARACTERISTIC_(TEMPERATURE_DISPLAY_UNITS, 0);
homekit_characteristic_t current_state = HOMEKIT_CHARACTERISTIC_(CURRENT_HEATING_COOLING_STATE, 0);
homekit_characteristic_t target_state = HOMEKIT_CHARACTERISTIC_(
    TARGET_HEATING_COOLING_STATE, 0, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_update)
);
homekit_characteristic_t cooling_threshold = HOMEKIT_CHARACTERISTIC_(
    COOLING_THRESHOLD_TEMPERATURE, 25, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_update)
);
homekit_characteristic_t heating_threshold = HOMEKIT_CHARACTERISTIC_(
    HEATING_THRESHOLD_TEMPERATURE, 15, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_update)
);
homekit_characteristic_t current_humidity = HOMEKIT_CHARACTERISTIC_(CURRENT_RELATIVE_HUMIDITY, 0);


void update_state() {
    uint8_t state = target_state.value.int_value;
    if ((state == 1 && current_temperature.value.float_value < target_temperature.value.float_value) ||
            (state == 3 && current_temperature.value.float_value < heating_threshold.value.float_value)) {
        if (current_state.value.int_value != 1) {
            current_state.value = HOMEKIT_UINT8(1);
            homekit_characteristic_notify(&current_state, current_state.value);

            heaterOn();
            coolerOff();
            fanOff();
            fanOn(HEATER_FAN_DELAY);
        }
    } else if ((state == 2 && current_temperature.value.float_value > target_temperature.value.float_value) ||
            (state == 3 && current_temperature.value.float_value > cooling_threshold.value.float_value)) {
        if (current_state.value.int_value != 2) {
            current_state.value = HOMEKIT_UINT8(2);
            homekit_characteristic_notify(&current_state, current_state.value);

            coolerOn();
            heaterOff();
            fanOff();
            fanOn(COOLER_FAN_DELAY);
        }
    } else {
        if (current_state.value.int_value != 0) {
            current_state.value = HOMEKIT_UINT8(0);
            homekit_characteristic_notify(&current_state, current_state.value);

            coolerOff();
            heaterOff();
            fanOff();
        }
    }
}


void temperature_sensor_task(void *_args) {
    sdk_os_timer_setfn(&fan_timer, fan_alarm, NULL);

    gpio_set_pullup(TEMPERATURE_SENSOR_PIN, false, false);

    gpio_enable(FAN_PIN, GPIO_OUTPUT);
    gpio_enable(HEATER_PIN, GPIO_OUTPUT);
    gpio_enable(COOLER_PIN, GPIO_OUTPUT);

    fanOff();
    heaterOff();
    coolerOff();

    float humidity_value, temperature_value;
    while (1) {
        bool success = dht_read_float_data(
            DHT_TYPE_DHT11, TEMPERATURE_SENSOR_PIN,
            &humidity_value, &temperature_value
        );
        if (success) {
            printf("Got readings: temperature %g, humidity %g\n", temperature_value, humidity_value);
            current_temperature.value = HOMEKIT_FLOAT(temperature_value);
            current_humidity.value = HOMEKIT_FLOAT(humidity_value);

            homekit_characteristic_notify(&current_temperature, current_temperature.value);
            homekit_characteristic_notify(&current_humidity, current_humidity.value);

            update_state();
        } else {
            printf("Couldnt read data from sensor\n");
        }

        vTaskDelay(TEMPERATURE_POLL_PERIOD / portTICK_PERIOD_MS);
    }
}

void thermostat_init() {
    xTaskCreate(temperature_sensor_task, "Thermostat", 256, NULL, 2, NULL);
}


homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_thermostat, .services=(homekit_service_t*[]) {
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Thermostat"),
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "HaPK"),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "001"),
            HOMEKIT_CHARACTERISTIC(MODEL, "MyThermostat"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, thermostat_identify),
            NULL
        }),
        HOMEKIT_SERVICE(THERMOSTAT, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Thermostat"),
            &current_temperature,
            &target_temperature,
            &current_state,
            &target_state,
            &cooling_threshold,
            &heating_threshold,
            &units,
            &current_humidity,
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

void user_init(void) {
    uart_set_baud(0, 115200);

    wifi_init();
    thermostat_init();
    homekit_server_init(&config);
}

