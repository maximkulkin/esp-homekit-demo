/*
 * This is example of animating a fire in a fireplace.
 *
 * Fireplace has a RGB LED (WS2812) strip layed out as a grid
 * 6 columns by 10 LEDs. LEDs start with first column going up,
 * then continue on next column going down and so on:
 *
 *   9 ->  10     29 ->
 * ^ 8   | 11   ^ 28
 * | .   |  .   |  .   ...
 * | 1   V 18   | 21
 *   0     19 ->  20
 *
 * Fireplace exposes itself as a HomeKit light bulb with
 * brightness setting.
 *
 * See demo.gif for demonstration.
 */
#include <stdio.h>
#include <string.h>
#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <ota-tftp.h>

#include <homekit/homekit.h>
#include <homekit/types.h>
#include <homekit/characteristics.h>

#include <ws2812_i2s/ws2812_i2s.h>

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

homekit_characteristic_t brightness = HOMEKIT_CHARACTERISTIC_(BRIGHTNESS, 50);


/* Board shape and size configuration. Sheild is 6x10, 60 pixels */
#define HEIGHT 10
#define WIDTH 6
#define NUM_LEDS (HEIGHT*WIDTH)

/* Refresh rate. Higher makes for flickerier
   Recommend small values for small displays */
#define FPS 17
#define FPS_DELAY (1000 / FPS / portTICK_PERIOD_MS)

/* Rate of cooling. Play with to change fire from
   roaring (larger values) to weak (smaller values) */
#define COOLING 55


ws2812_pixel_t heat_colors[16] = {
    { .color=0x000000 },
    { .color=0x330000 },
    { .color=0x660000 },
    { .color=0x990000 },
    { .color=0xcc0000 },
    { .color=0xff0000 },
    { .color=0xff3300 },
    { .color=0xff6600 },
    { .color=0xff9900 },
    { .color=0xffcc00 },
    { .color=0xffff00 },
    { .color=0xffff33 },
    { .color=0xffff66 },
    { .color=0xffff99 },
    { .color=0xffffcc },
    { .color=0xffffff },
};

static int min(int a, int b) {
    return (a > b) ? b : a;
}

uint8_t scale(uint8_t x, uint8_t s) {
    return (((uint16_t)x) * s) >> 8;
}

ws2812_pixel_t heat_color(uint8_t index) {
    // Since palette is only 16 colors, uses high 4 bits if index
    // to pick to palette colors and lower 4 bits to interpolate
    // between those two colors.
    ws2812_pixel_t lo_color = heat_colors[index >> 4];
    if (!(index & 0xf))
        return lo_color;

    ws2812_pixel_t hi_color = heat_colors[min((index >> 4) + 1, 15)];
    uint8_t s2 = (index & 0xf) << 4;
    uint8_t s1 = 255 - s2;

    return (ws2812_pixel_t) {
        .red = scale(lo_color.red, s1) + scale(hi_color.red, s2),
        .green = scale(lo_color.green, s1) + scale(hi_color.green, s2),
        .blue = scale(lo_color.blue, s1) + scale(hi_color.blue, s2),
    };
}



ws2812_pixel_t pixels[NUM_LEDS];
bool fireplace_on = false;

void fireplace_update() {
    // Update fire animation
    static unsigned int stack[WIDTH][HEIGHT] = {};

    unsigned int hot = 256 * brightness.value.int_value / 100;
    unsigned int maxhot = hot * HEIGHT;

    // 1. Cool all the sparks
    for(int i=0; i < WIDTH; i++) {
        for (int j=0; j < HEIGHT; j++) {
            unsigned int cooling = hwrand() % COOLING;
            stack[i][j] = (stack[i][j] < cooling) ? 0 : stack[i][j] - cooling;
        }

        if (stack[i][0] < hot) {
            stack[i][0] = hot + hwrand() % (maxhot - hot);
        }
    }

    for( int i = 0; i < WIDTH; i++) {
        for( int j = HEIGHT-1; j > 0; j--) {
            unsigned long heat = stack[i][j] + stack[i][j-1];
            if (i > 0)
                heat += stack[i-1][j-1];
            if (i < WIDTH-1)
                heat += stack[i+1][j-1];

            stack[i][j] = heat / 6;
        }
    }

    for (int i = 0; i < WIDTH; i++) {
        for (int j = 0; j < HEIGHT; j++) {
            uint8_t index = ((unsigned long)stack[i][j]) / HEIGHT * 2;
            ws2812_pixel_t color = heat_color(index);

            if (i % 2 == 0) {
                pixels[(i*HEIGHT) + j] = color;
            } else {
                pixels[(i*HEIGHT) + HEIGHT - j - 1] = color;
            }
        }
    }

    ws2812_i2s_update(pixels, PIXEL_RGB);
}

void fireplace_clear() {
    memset(pixels, 0, sizeof(pixels));
    ws2812_i2s_update(pixels, PIXEL_RGB);
}

void fireplace_task(void *_arg) {
    while (fireplace_on) {
        fireplace_update();

        vTaskDelay(FPS_DELAY);
    }

    fireplace_clear();
    vTaskDelete(NULL);
}

void fireplace_init() {
    ws2812_i2s_init(NUM_LEDS, PIXEL_RGB);
    memset(pixels, 0, sizeof(pixels));
}

void fireplace_start() {
    fireplace_on = true;
    xTaskCreate(fireplace_task, "Fireplace", 256, NULL, 2, NULL);
}

void _fill_column(int column, ws2812_pixel_t color) {
    if (column % 2 == 0) {
        for (int j = 0; j < HEIGHT; j++)
            pixels[(column*HEIGHT) + j] = color;
    } else {
        for (int j = 0; j < HEIGHT; j++)
            pixels[(column*HEIGHT) + HEIGHT - j - 1] = color;
    }
}

void fireplace_identify_task(void *_args) {
    bool old_on = fireplace_on;
    fireplace_on = false;
    vTaskDelay(2*FPS_DELAY);

    ws2812_pixel_t black = { .color=0x000000 };
    ws2812_pixel_t red = { .color=0x990000 };

    memset(pixels, 0, sizeof(pixels));
    ws2812_i2s_update(pixels, PIXEL_RGB);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    for (int x = 0; x < 2; x++) {
        for (int i = 0; i < WIDTH; i++) {
            _fill_column(i, red);
            ws2812_i2s_update(pixels, PIXEL_RGB);

            vTaskDelay(100 / portTICK_PERIOD_MS);
            _fill_column(i, black);
        }

        for (int i = WIDTH-2; i > 0; i--) {
            _fill_column(i, red);
            ws2812_i2s_update(pixels, PIXEL_RGB);

            vTaskDelay(100 / portTICK_PERIOD_MS);
            _fill_column(i, black);
        }
    }

    ws2812_i2s_update(pixels, PIXEL_RGB);

    if (old_on)
        fireplace_start();

    vTaskDelete(NULL);
}

void fireplace_identify(homekit_value_t _value) {
    printf("Fireplace identify\n");
    xTaskCreate(fireplace_identify_task, "Fireplace identify", 256, NULL, 2, NULL);
}

homekit_value_t fireplace_on_get() {
    return HOMEKIT_BOOL(fireplace_on);
}

void fireplace_on_set(homekit_value_t value) {
    if (value.format != homekit_format_bool) {
        printf("Invalid value for fireplace ON characteristic: type=%d\n", value.format);
        return;
    }

    if (value.bool_value && !fireplace_on) {
        fireplace_start();
    }
    fireplace_on = value.bool_value;
}


homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_lightbulb, .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Fireplace"),
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "HaPK"),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "001"),
            HOMEKIT_CHARACTERISTIC(MODEL, "LEDFireplace"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, fireplace_identify),
            NULL
        }),
        HOMEKIT_SERVICE(LIGHTBULB, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Fireplace"),
            HOMEKIT_CHARACTERISTIC(
                ON, false,
                .getter=fireplace_on_get,
                .setter=fireplace_on_set
            ),
            &brightness,
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
    fireplace_init();
    fireplace_start();
    homekit_server_init(&config);
}
