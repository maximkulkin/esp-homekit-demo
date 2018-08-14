/*
 * Example of using esp-homekit library to control
 * a simple $5 Sonoff Led using HomeKit.
 * The esp-wifi-config library is also used in this
 * example. This means you don't have to specify
 * your network's SSID and password before building.
 *
 * In order to flash the sonoff basic you will have to
 * have a 3,3v (logic level) FTDI adapter.
 *
 * To flash this example connect 3,3v, TX, RX, GND
 * in this order, beginning in the (square) pin header
 * next to the button.
 * Next hold down the button and connect the FTDI adapter
 * to your computer. The sonoff is now in flash mode and
 * you can flash the custom firmware.
 *
 * WARNING: Do not connect the sonoff to AC while it's
 * connected to the FTDI adapter! This may fry your
 * computer and sonoff.
 *
modified to do pulse-width-modulation (PWM) of LED
 */

#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include "multipwm.h"

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>
#include "wifi.h"

#include "button.h"

// The GPIO pin that is connected to the LED on the Sonoff Basic.
// The GPIO pin that is connected to the button on the Sonoff Basic.
const int button_gpio = 0;

float warm_bri;
float cold_bri;
bool warm_on;
bool cold_on;
//const int warm_led_pin = 14;
//const int cold_led_pin = 12;
uint8_t pins[] = {14, 12};
uint32_t duties[] = {0, 0};
pwm_info_t pwm_info;

void multipwm_set_task()
{
    multipwm_stop(&pwm_info);
    for (uint8_t ii=0; ii<pwm_info.channels; ii++) {
        multipwm_set_duty(&pwm_info, ii, duties[ii]);
    }
    multipwm_start(&pwm_info);
}

void reset_configuration_task() {
    //Flash the LED first before we start the reset
    for (int i=0; i<3; i++) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    printf("Resetting Wifi Config\n");
    wifi_config_reset();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    printf("Resetting HomeKit Config\n");
    homekit_server_reset();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
     
    printf("Restarting\n");
    sdk_system_restart();
    vTaskDelete(NULL);
}

void reset_configuration() {
    printf("Resetting Sonoff configuration\n");
    xTaskCreate(reset_configuration_task, "Reset configuration", 256, NULL, 2, NULL);
}


static void wifi_init() {
    struct sdk_station_config wifi_config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASSWORD,
    };
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&wifi_config);
    sdk_wifi_station_connect();
}


void gpio_init() {
    pwm_info.channels = 2;

    multipwm_init(&pwm_info);
    for (uint8_t ii=0; ii<pwm_info.channels; ii++) {
        multipwm_set_pin(&pwm_info, ii, pins[ii]);
    }
}


void lightSET_task(void *pvParameters) {
    duties[0] = warm_on ? (UINT16_MAX*warm_bri/100) : 0;
    duties[1] = cold_on ? (UINT16_MAX*cold_bri/100) : 0;
    multipwm_set_task();
    vTaskDelete(NULL);
}


void lightSET() {
    xTaskCreate(lightSET_task, "Light Set", 256, NULL, 2, NULL);
}


void light_init() {
    printf("light_init:\n");
    warm_on=false;
    cold_on=false;
    warm_bri=0;
    cold_bri=0;
    lightSET();
}


homekit_value_t warm_light_on_get() { return HOMEKIT_BOOL(warm_on); }
homekit_value_t cold_light_on_get() { return HOMEKIT_BOOL(cold_on); }

void warm_light_on_set(homekit_value_t value) {
    if (value.format != homekit_format_bool) {
        printf("Invalid on-value format: %d\n", value.format);
        return;
    }
    warm_on = value.bool_value;
    lightSET();
}
void cold_light_on_set(homekit_value_t value) {
    if (value.format != homekit_format_bool) {
        printf("Invalid on-value format: %d\n", value.format);
        return;
    }
    cold_on = value.bool_value;
    lightSET();
}

homekit_value_t light_warm_bri_get() { return HOMEKIT_INT(warm_bri); }
homekit_value_t light_cold_bri_get() { return HOMEKIT_INT(cold_bri); }

void light_warm_bri_set(homekit_value_t value) {
    if (value.format != homekit_format_int) {
        printf("Invalid warm-value format: %d\n", value.format);
        return;
    }
    warm_bri = value.int_value;
    lightSET();
}

void light_cold_bri_set(homekit_value_t value) {
    if (value.format != homekit_format_int) {
        printf("Invalid cold-value format: %d\n", value.format);
        return;
    }
    cold_bri = value.int_value;
    lightSET();
}


void light_identify_task(void *_args) {
    //Identify Sonoff by Pulsing LED.
    for (int j=0; j<3; j++) {
        for (int j=0; j<2; j++) {
            for (int i=0; i<=40; i++) {
                int w;
                float b;
                w = (UINT16_MAX - UINT16_MAX*i/20);
                if(i>20) {
                    w = (UINT16_MAX - UINT16_MAX*abs(i-40)/20);
                }
                b = 100.0*(UINT16_MAX-w)/UINT16_MAX;
                duties[0] = w;
                duties[1] = w;
                multipwm_set_task();
                printf("Light_Identify: i = %2d b = %3.0f w = %5d\n",i, b, UINT16_MAX);
                vTaskDelay(20 / portTICK_PERIOD_MS);
            }
        }
    vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    lightSET();
    vTaskDelete(NULL);
}


void light_identify(homekit_value_t _value) {
    printf("Light Identify\n");
    xTaskCreate(light_identify_task, "Light identify", 256, NULL, 2, NULL);
}


homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Sonoff Led");

homekit_characteristic_t warm_light_on = HOMEKIT_CHARACTERISTIC_(ON, true, .getter=warm_light_on_get, .setter=warm_light_on_set);
homekit_characteristic_t cold_light_on = HOMEKIT_CHARACTERISTIC_(ON, true, .getter=cold_light_on_get, .setter=cold_light_on_set);


void button_callback(uint8_t gpio, button_event_t event) {
    switch (event) {
        case button_event_single_press:
            printf("Toggling lightbulb due to button at GPIO %2d\n", gpio);
            warm_light_on.value.bool_value = !warm_light_on.value.bool_value;
            warm_on = warm_light_on.value.bool_value;
            cold_light_on.value.bool_value = !cold_light_on.value.bool_value;
            cold_on = cold_light_on.value.bool_value;
            lightSET();
            homekit_characteristic_notify(&warm_light_on, warm_light_on.value);
            homekit_characteristic_notify(&cold_light_on, cold_light_on.value);
            break;
        case button_event_long_press:
            printf("Reseting WiFi configuration!\n");
            reset_configuration();
            break;
        default:
            printf("Unknown button event: %d\n", event);
    }
}

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(
        .id=1,
        .category=homekit_accessory_category_lightbulb,
        .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION,
            .characteristics=(homekit_characteristic_t*[]){
                &name,
                HOMEKIT_CHARACTERISTIC(MANUFACTURER, "iTEAD"),
                HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "PWM Dimmer"),
                HOMEKIT_CHARACTERISTIC(MODEL, "Sonoff Led"),
                HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1.6"),
                HOMEKIT_CHARACTERISTIC(IDENTIFY, light_identify),
                NULL
            }),
        HOMEKIT_SERVICE(LIGHTBULB, .primary=true,
            .characteristics=(homekit_characteristic_t*[]){
                HOMEKIT_CHARACTERISTIC(NAME, "Cold light"),
                &cold_light_on,
                HOMEKIT_CHARACTERISTIC(BRIGHTNESS, 100, .getter=light_cold_bri_get, .setter=light_cold_bri_set),
            NULL
        }),
        HOMEKIT_SERVICE(LIGHTBULB,
            .characteristics=(homekit_characteristic_t*[]){
                HOMEKIT_CHARACTERISTIC(NAME, "Warm light"),
                &warm_light_on,
                HOMEKIT_CHARACTERISTIC(BRIGHTNESS, 100, .getter=light_warm_bri_get, .setter=light_warm_bri_set),
            NULL
        }),
        NULL
    }),
    NULL
};


homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111"    //easy for testing
};

void on_wifi_ready() {
    homekit_server_init(&config);
}

void create_accessory_name() {
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);
    
    int name_len = snprintf(NULL, 0, "Sonoff Led %02X:%02X:%02X",
            macaddr[3], macaddr[4], macaddr[5]);
    char *name_value = malloc(name_len+1);
    snprintf(name_value, name_len+1, "Sonoff Led %02X:%02X:%02X",
            macaddr[3], macaddr[4], macaddr[5]);
    
    name.value = HOMEKIT_STRING(name_value);
}


void user_init(void) {
    uart_set_baud(0, 115200);
    create_accessory_name();


    wifi_init();                                                   //testing
    homekit_server_init(&config);                                  //testing

    //wifi_config_init("Sonoff Dimmer", NULL, on_wifi_ready);        //release
    
    gpio_init();
    light_init();

    if (button_create(button_gpio, 0, 4000, button_callback)) {
        printf("Failed to initialize button\n");
    }
}