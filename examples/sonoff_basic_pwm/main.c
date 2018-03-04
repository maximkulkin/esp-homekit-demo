/*
 * Example of using esp-homekit library to control
 * a simple $5 Sonoff Basic using HomeKit.
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

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>
#include "wifi.h"

#include "button.h"
#include "toggle.h"

// The GPIO pin that is connected to the relay on the Sonoff Basic.
const int relay_gpio = 12;
// The GPIO pin that is connected to the LED on the Sonoff Basic.
const int led_gpio = 13;
// The GPIO pin that is connected to the button on the Sonoff Basic.
const int button_gpio = 0;
// The GPIO pin that is connected to the header on the Sonoff Basic (external switch).
const int toggle_gpio = 14;

#include <pwm.h>
// The PWM pin that is connected to the PWM daughter board.
const int pwm_gpio = 13;

const bool dev = true;

float bri;
bool on;
uint8_t pins[1];
//void toggle_callback(uint8_t gpio);  // as this needed

void led_write(bool on) {
    gpio_write(led_gpio, on ? 0 : 1);
}


void reset_configuration_task() {
    //Flash the LED first before we start the reset
    for (int i=0; i<3; i++) {
        led_write(true);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        led_write(false);
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
//    gpio_enable(relay_gpio, GPIO_OUTPUT);
    gpio_enable(toggle_gpio, GPIO_INPUT);
    pins[0] = led_gpio;
    pwm_init(1, pins, false);
}


void lightSET_task(void *pvParameters) {
    int w;
    if (on) {
        w = (UINT16_MAX - UINT16_MAX*bri/100);
        pwm_set_duty(w);
        printf("ON  %3d [%5d]\n", (int)bri , w);
    } else {
        printf("OFF\n");
        pwm_set_duty(UINT16_MAX);
    }
    vTaskDelete(NULL);
}


void lightSET() {
    xTaskCreate(lightSET_task, "Light Set", 256, NULL, 2, NULL);
}


void light_init() {
    printf("light_init:\n");
    on=false;
    bri=100;
    printf("on = false  bri = 100 %%\n");
    pwm_set_freq(1000);
    printf("PWMpwm_set_freq = 1000 Hz  pwm_set_duty = 0 = 0%%\n");
    pwm_set_duty(UINT16_MAX);
    pwm_start();
    lightSET();
}


homekit_value_t light_on_get() { return HOMEKIT_BOOL(on); }

void light_on_set(homekit_value_t value) {
    if (value.format != homekit_format_bool) {
        printf("Invalid on-value format: %d\n", value.format);
        return;
    }
    on = value.bool_value;
    lightSET();
}

homekit_value_t light_bri_get() { return HOMEKIT_INT(bri); }

void light_bri_set(homekit_value_t value) {
    if (value.format != homekit_format_int) {
        printf("Invalid bri-value format: %d\n", value.format);
        return;
    }
    bri = value.int_value;
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
                pwm_set_duty(w);
                printf("Light_Identify: i = %2d b = %3.0f w = %5d\n",i, b, UINT16_MAX);
                vTaskDelay(20 / portTICK_PERIOD_MS);
            }
        }
    vTaskDelay(500 / portTICK_PERIOD_MS);
    }
    pwm_set_duty(0);
    lightSET();
    vTaskDelete(NULL);
}


void light_identify(homekit_value_t _value) {
    printf("Light Identify\n");
    xTaskCreate(light_identify_task, "Light identify", 256, NULL, 2, NULL);
}


homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Sonoff Dimmer");

homekit_characteristic_t lightbulb_on = HOMEKIT_CHARACTERISTIC_(ON, false, .getter=light_on_get, .setter=light_on_set);


void button_callback(uint8_t gpio, button_event_t event) {
    switch (event) {
        case button_event_single_press:
            printf("Toggling lightbulb due to button at GPIO %2d\n", gpio);
            lightbulb_on.value.bool_value = !lightbulb_on.value.bool_value;
            on = lightbulb_on.value.bool_value;
            lightSET();
            homekit_characteristic_notify(&lightbulb_on, lightbulb_on.value);
            break;
        case button_event_long_press:
            printf("Reseting WiFi configuration!\n");
            reset_configuration();
            break;
        default:
            printf("Unknown button event: %d\n", event);
    }
}


void toggle_callback(uint8_t gpio) {
    printf("Toggling lightbulb due to switch at GPIO %2d\n", gpio);
    lightbulb_on.value.bool_value = !lightbulb_on.value.bool_value;
    on = lightbulb_on.value.bool_value;
    lightSET();
    homekit_characteristic_notify(&lightbulb_on, lightbulb_on.value);
}


homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(
        .id=1,
        .category=homekit_accessory_category_switch,
        .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION,
            .characteristics=(homekit_characteristic_t*[]){
                &name,
                HOMEKIT_CHARACTERISTIC(MANUFACTURER, "iTEAD"),
                HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "PWM Dimmer"),
                HOMEKIT_CHARACTERISTIC(MODEL, "Sonoff Basic"),
                HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1.6"),
                HOMEKIT_CHARACTERISTIC(IDENTIFY, light_identify),
                NULL
            }),
        HOMEKIT_SERVICE(LIGHTBULB, .primary=true,
            .characteristics=(homekit_characteristic_t*[]){
                HOMEKIT_CHARACTERISTIC(NAME, "Sonoff Dimmer"),
                &lightbulb_on,
                HOMEKIT_CHARACTERISTIC(BRIGHTNESS, 100, .getter=light_bri_get, .setter=light_bri_set),
            NULL
        }),
        NULL
    }),
    NULL
};


homekit_server_config_t config = {
    .accessories = accessories,
    .password = "190-11-978"    //valid for release
//    .password = "111-11-111"    //easy for testing
};

void on_wifi_ready() {
    homekit_server_init(&config);
}

void create_accessory_name() {
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);
    
    int name_len = snprintf(NULL, 0, "Sonoff Dimmer %02X:%02X:%02X",
            macaddr[3], macaddr[4], macaddr[5]);
    char *name_value = malloc(name_len+1);
    snprintf(name_value, name_len+1, "Sonoff Dimmer %02X:%02X:%02X",
            macaddr[3], macaddr[4], macaddr[5]);
    
    name.value = HOMEKIT_STRING(name_value);
}


void user_init(void) {
    uart_set_baud(0, 115200);
    create_accessory_name();

/*
    wifi_init();                                                   //testing
    homekit_server_init(&config);                                  //testing
 */
    wifi_config_init("Sonoff Dimmer", NULL, on_wifi_ready);        //release
    
    gpio_init();
    light_init();

    if (button_create(button_gpio, 0, 4000, button_callback)) {
        printf("Failed to initialize button\n");
    }
    if (toggle_create(toggle_gpio, toggle_callback)) {
        printf("Failed to initialize toggle\n");
    }
}
