/*
* This is an example of an rgb ws2812_i2s led strip animation using a WS2812FX library fork for freertos
*
* NOTE:
*    1) the ws2812_i2s library uses hardware I2S so output pin is GPIO3 and cannot be changed.
*    2) on some ESP8266 such as the Wemos D1 mini, GPIO3 is the same pin used for serial comms (RX pin).
*    3) you can still print stuff to serial but transmiting data to wemos will interfere on the leds output
* 
* Debugging printf statements are disabled below because of note (2) - you can uncomment
* them if your hardware supports serial comms that do not conflict with I2S on GPIO3.
*
* Contributed April 2018 by https://github.com/PCSaito
*/
#include <stdio.h>
#include <stdlib.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <math.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include "wifi.h"

#include "WS2812FX/WS2812FX.h"

#define LED_RGB_SCALE 255       // this is the scaling factor used for color conversion
#define LED_COUNT 50            // this is the number of WS2812B leds on the strip
#define LED_INBUILT_GPIO 2      // this is the onboard LED used to show on/off only

// Global variables
float led_hue = 0;              // hue is scaled 0 to 360
float led_saturation = 100;      // saturation is scaled 0 to 100
float led_brightness = 33;     // brightness is scaled 0 to 100
bool led_on = false;            // on is boolean on or off
bool led_on_value = (bool)0;                // this is the value to write to GPIO for led on (0 = GPIO low)

float fx_hue = 64;              // hue is scaled 0 to 360
float fx_saturation = 50;      // saturation is scaled 0 to 100
float fx_brightness = 50;     // brightness is scaled 0 to 100
bool fx_on = true;

//http://blog.saikoled.com/post/44677718712/how-to-convert-from-hsi-to-rgb-white
static void hsi2rgb(float h, float s, float i, ws2812_pixel_t* rgb) {
    int r, g, b;

    while (h < 0) { h += 360.0F; };     // cycle h around to 0-360 degrees
    while (h >= 360) { h -= 360.0F; };
    h = 3.14159F*h / 180.0F;            // convert to radians.
    s /= 100.0F;                        // from percentage to ratio
    i /= 100.0F;                        // from percentage to ratio
    s = s > 0 ? (s < 1 ? s : 1) : 0;    // clamp s and i to interval [0,1]
    i = i > 0 ? (i < 1 ? i : 1) : 0;    // clamp s and i to interval [0,1]
    i = i * sqrt(i);                    // shape intensity to have finer granularity near 0

    if (h < 2.09439) {
        r = LED_RGB_SCALE * i / 3 * (1 + s * cos(h) / cos(1.047196667 - h));
        g = LED_RGB_SCALE * i / 3 * (1 + s * (1 - cos(h) / cos(1.047196667 - h)));
        b = LED_RGB_SCALE * i / 3 * (1 - s);
    }
    else if (h < 4.188787) {
        h = h - 2.09439;
        g = LED_RGB_SCALE * i / 3 * (1 + s * cos(h) / cos(1.047196667 - h));
        b = LED_RGB_SCALE * i / 3 * (1 + s * (1 - cos(h) / cos(1.047196667 - h)));
        r = LED_RGB_SCALE * i / 3 * (1 - s);
    }
    else {
        h = h - 4.188787;
        b = LED_RGB_SCALE * i / 3 * (1 + s * cos(h) / cos(1.047196667 - h));
        r = LED_RGB_SCALE * i / 3 * (1 + s * (1 - cos(h) / cos(1.047196667 - h)));
        g = LED_RGB_SCALE * i / 3 * (1 - s);
    }

    rgb->red = (uint8_t) r;
    rgb->green = (uint8_t) g;
    rgb->blue = (uint8_t) b;
    rgb->white = (uint8_t) 0;           // white channel is not used
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


void led_identify_task(void *_args) {
    // initialise the onboard led as a secondary indicator (handy for testing)
    gpio_enable(LED_INBUILT_GPIO, GPIO_OUTPUT);
    
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            gpio_write(LED_INBUILT_GPIO, (int)led_on_value);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            gpio_write(LED_INBUILT_GPIO, 1 - (int)led_on_value);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }

    gpio_write(LED_INBUILT_GPIO, 1 - (int)led_on_value);

    vTaskDelete(NULL);
}

void led_identify(homekit_value_t _value) {
    // printf("LED identify\n");
    xTaskCreate(led_identify_task, "LED identify", 128, NULL, 2, NULL);
}

homekit_value_t led_on_get() {
    return HOMEKIT_BOOL(led_on);
}

void led_on_set(homekit_value_t value) {
    if (value.format != homekit_format_bool) {
        // printf("Invalid on-value format: %d\n", value.format);
        return;
    }

    led_on = value.bool_value;
    
    if (led_on) {
        WS2812FX_setBrightness((uint8_t)floor(led_brightness*2.55));
        
    } else {
        WS2812FX_setBrightness(0);
    }
}

homekit_value_t led_brightness_get() {
    return HOMEKIT_INT(led_brightness);
}

void led_brightness_set(homekit_value_t value) {
    if (value.format != homekit_format_int) {
        // printf("Invalid brightness-value format: %d\n", value.format);
        return;
    }
    led_brightness = value.int_value;
    
    WS2812FX_setBrightness((uint8_t)floor(led_brightness*2.55));
}

homekit_value_t led_hue_get() {
    return HOMEKIT_FLOAT(led_hue);
}

void led_hue_set(homekit_value_t value) {
    if (value.format != homekit_format_float) {
        // printf("Invalid hue-value format: %d\n", value.format);
        return;
    }
    led_hue = value.float_value;
    
    ws2812_pixel_t rgb = { { 0, 0, 0, 0 } };
    hsi2rgb(led_hue, led_saturation, 100, &rgb);
    
    WS2812FX_setColor(rgb.red, rgb.green, rgb.blue);
}

homekit_value_t led_saturation_get() {
    return HOMEKIT_FLOAT(led_saturation);
}

void led_saturation_set(homekit_value_t value) {
    if (value.format != homekit_format_float) {
        // printf("Invalid sat-value format: %d\n", value.format);
        return;
    }
    led_saturation = value.float_value;
    
    ws2812_pixel_t rgb = { { 0, 0, 0, 0 } };
    hsi2rgb(led_hue, led_saturation, 100, &rgb);
    
    WS2812FX_setColor(rgb.red, rgb.green, rgb.blue);
}

homekit_value_t fx_on_get() {
    return HOMEKIT_BOOL(fx_on);
}

void fx_on_set(homekit_value_t value) {
    if (value.format != homekit_format_bool) {
        // printf("Invalid on-value format: %d\n", value.format);
        return;
    }
    fx_on = value.bool_value;
    
    if (fx_on) {
        WS2812FX_setMode360(fx_hue);
    } else {
        WS2812FX_setMode360(0);
    }
}

homekit_value_t fx_brightness_get() {
    return HOMEKIT_INT(fx_brightness);
}

void fx_brightness_set(homekit_value_t value) {
    if (value.format != homekit_format_int) {
        // printf("Invalid brightness-value format: %d\n", value.format);
        return;
    }
    fx_brightness = value.int_value;
    
    if (fx_brightness > 50) {
        uint8_t fx_speed = fx_brightness - 50;
        WS2812FX_setSpeed(fx_speed*5.1);
        WS2812FX_setInverted(true);
    } else {
        uint8_t fx_speed = abs(fx_brightness - 51);
        WS2812FX_setSpeed(fx_speed*5.1);
        WS2812FX_setInverted(false);
    }
}

homekit_value_t fx_hue_get() {
    return HOMEKIT_FLOAT(fx_hue);
}

void fx_hue_set(homekit_value_t value) {
    if (value.format != homekit_format_float) {
        // printf("Invalid hue-value format: %d\n", value.format);
        return;
    }
    fx_hue = value.float_value;
    
    WS2812FX_setMode360(fx_hue);
}

homekit_value_t fx_saturation_get() {
    return HOMEKIT_FLOAT(fx_saturation);
}

void fx_saturation_set(homekit_value_t value) {
    if (value.format != homekit_format_float) {
        // printf("Invalid hue-value format: %d\n", value.format);
        return;
    }
    fx_saturation = value.float_value;    
}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Chihiro");

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id = 1, .category = homekit_accessory_category_lightbulb, .services = (homekit_service_t*[]) {
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]) {
            &name,
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Generic"),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "137A2BABF19D"),
            HOMEKIT_CHARACTERISTIC(MODEL, "LEDStripFX"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, led_identify),
            NULL
        }),
        HOMEKIT_SERVICE(LIGHTBULB, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Chihiro"),
            HOMEKIT_CHARACTERISTIC(
                ON, true,
            .getter = led_on_get,
            .setter = led_on_set
                ),
            HOMEKIT_CHARACTERISTIC(
                BRIGHTNESS, 100,
            .getter = led_brightness_get,
            .setter = led_brightness_set
                ),
            HOMEKIT_CHARACTERISTIC(
                HUE, 0,
            .getter = led_hue_get,
            .setter = led_hue_set
                ),
            HOMEKIT_CHARACTERISTIC(
                SATURATION, 0,
            .getter = led_saturation_get,
            .setter = led_saturation_set
                ),
            NULL
        }),
        HOMEKIT_SERVICE(LIGHTBULB, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Chihiro FX"),
            HOMEKIT_CHARACTERISTIC(
                ON, true,
            .getter = fx_on_get,
            .setter = fx_on_set
                ),
            HOMEKIT_CHARACTERISTIC(
                BRIGHTNESS, 100,
            .getter = fx_brightness_get,
            .setter = fx_brightness_set
                ),
            HOMEKIT_CHARACTERISTIC(
                HUE, 0,
            .getter = fx_hue_get,
            .setter = fx_hue_set
                ),
            HOMEKIT_CHARACTERISTIC(
                SATURATION, 0,
            .getter = fx_saturation_get,
            .setter = fx_saturation_set
                ),
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
    // uart_set_baud(0, 115200);

    // This example shows how to use same firmware for multiple similar accessories
    // without name conflicts. It uses the last 3 bytes of accessory's MAC address as
    // accessory name suffix.
    uint8_t macaddr[6];
    sdk_wifi_get_macaddr(STATION_IF, macaddr);
    int name_len = snprintf(NULL, 0, "Chihiro-%02X%02X%02X", macaddr[3], macaddr[4], macaddr[5]);
    char *name_value = malloc(name_len + 1);
    snprintf(name_value, name_len + 1, "Chihiro-%02X%02X%02X", macaddr[3], macaddr[4], macaddr[5]);
    name.value = HOMEKIT_STRING(name_value);

    wifi_init();
    WS2812FX_init(LED_COUNT);
    homekit_server_init(&config);
    
    led_identify(HOMEKIT_INT(led_brightness));
}