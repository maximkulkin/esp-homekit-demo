/*
 * Example of using per-client random passwords.
 *
 * Each time client starts pairing, it generates random
 * password for each client. It uses SSD1306 OLED display
 * to show password during pairing process.
 *
 * SSD1306 is connected via I2C interface:
 *   SDA -> GPIO4
 *   SCL -> GPIO5
 */

#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <esp/hwrand.h>

#include <qrcode.h>
#include <i2c/i2c.h>
#include <ssd1306/ssd1306.h>
#include <fonts/fonts.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include "wifi.h"


#define QRCODE_VERSION 2

#define I2C_BUS 0
#define I2C_SDA_PIN 4
#define I2C_SCL_PIN 5

#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64
#define DEFAULT_FONT FONT_FACE_TERMINUS_BOLD_12X24_ISO8859_1
// #define DEFAULT_FONT FONTS_TERMINUS_6X12_ISO8859_1

static const ssd1306_t display = {
    .protocol = SSD1306_PROTO_I2C,
    .screen = SSD1306_SCREEN,
    .i2c_dev.bus = I2C_BUS,
    .i2c_dev.addr = SSD1306_I2C_ADDR_0,
    .width = DISPLAY_WIDTH,
    .height = DISPLAY_HEIGHT,
};

static uint8_t display_buffer[DISPLAY_WIDTH * DISPLAY_HEIGHT / 8];

void display_init() {
    i2c_init(I2C_BUS, I2C_SCL_PIN, I2C_SDA_PIN, I2C_FREQ_400K);
    if (ssd1306_init(&display)) {
        printf("Failed to initialize OLED display\n");
        return;
    }
    ssd1306_set_whole_display_lighting(&display, false);
    ssd1306_set_scan_direction_fwd(&display, false);
    ssd1306_set_segment_remapping_enabled(&display, true);
}

bool password_displayed = false;
void display_password(const char *password) {
    ssd1306_display_on(&display, true);

    ssd1306_fill_rectangle(&display, display_buffer, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, OLED_COLOR_BLACK);
    ssd1306_draw_string(&display, display_buffer, font_builtin_fonts[DEFAULT_FONT], 4, 20, (char*)password, OLED_COLOR_WHITE, OLED_COLOR_BLACK);

    ssd1306_load_frame_buffer(&display, display_buffer);

    password_displayed = true;
}

void hide_password() {
    if (!password_displayed)
        return;

    ssd1306_clear_screen(&display);
    ssd1306_display_on(&display, false);

    password_displayed = false;
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


void on_password(const char *password) {
    display_password(password);
}


void on_homekit_event(homekit_event_t event) {
    hide_password();
}


homekit_server_config_t config = {
    .accessories = accessories,
    .password_callback = on_password,
    .on_event = on_homekit_event,
};

void user_init(void) {
    uart_set_baud(0, 115200);

    display_init();

    wifi_init();
    led_init();

    homekit_server_init(&config);
}
