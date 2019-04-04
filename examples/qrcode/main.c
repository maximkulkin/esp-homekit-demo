/*
 * Example of using random passwords and setup IDs.
 *
 * Each time device starts, it generates random
 * password and setup ID. It uses SSD1306 OLED display
 * to show password and pairing QR code.
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
#define DEFAULT_FONT FONT_FACE_TERMINUS_6X12_ISO8859_1

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

void display_draw_pixel(uint8_t x, uint8_t y, bool white) {
    ssd1306_color_t color = white ? OLED_COLOR_WHITE : OLED_COLOR_BLACK;
    ssd1306_draw_pixel(&display, display_buffer, x, y, color);
}

void display_draw_pixel_2x2(uint8_t x, uint8_t y, bool white) {
    ssd1306_color_t color = white ? OLED_COLOR_WHITE : OLED_COLOR_BLACK;

    ssd1306_draw_pixel(&display, display_buffer, x, y, color);
    ssd1306_draw_pixel(&display, display_buffer, x+1, y, color);
    ssd1306_draw_pixel(&display, display_buffer, x, y+1, color);
    ssd1306_draw_pixel(&display, display_buffer, x+1, y+1, color);
}

void display_draw_qrcode(QRCode *qrcode, uint8_t x, uint8_t y, uint8_t size) {
    void (*draw_pixel)(uint8_t x, uint8_t y, bool white) = display_draw_pixel;
    if (size >= 2) {
        draw_pixel = display_draw_pixel_2x2;
    }

    uint8_t cx;
    uint8_t cy = y;

    cx = x + size;
    draw_pixel(x, cy, 1);
    for (uint8_t i = 0; i < qrcode->size; i++, cx+=size)
        draw_pixel(cx, cy, 1);
    draw_pixel(cx, cy, 1);

    cy += size;

    for (uint8_t j = 0; j < qrcode->size; j++, cy+=size) {
      cx = x + size;
      draw_pixel(x, cy, 1);
      for (uint8_t i = 0; i < qrcode->size; i++, cx+=size) {
          draw_pixel(cx, cy, qrcode_getModule(qrcode, i, j)==0);
      }
      draw_pixel(cx, cy, 1);
    }

    cx = x + size;
    draw_pixel(x, cy, 1);
    for (uint8_t i = 0; i < qrcode->size; i++, cx+=size)
        draw_pixel(cx, cy, 1);
    draw_pixel(cx, cy, 1);
}

bool qrcode_shown = false;
void qrcode_show(homekit_server_config_t *config) {
    char setupURI[20];
    homekit_get_setup_uri(config, setupURI, sizeof(setupURI));

    QRCode qrcode;

    uint8_t *qrcodeBytes = malloc(qrcode_getBufferSize(QRCODE_VERSION));
    qrcode_initText(&qrcode, qrcodeBytes, QRCODE_VERSION, ECC_MEDIUM, setupURI);

    qrcode_print(&qrcode);  // print on console

    ssd1306_display_on(&display, true);

    ssd1306_fill_rectangle(&display, display_buffer, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, OLED_COLOR_BLACK);
    ssd1306_draw_string(&display, display_buffer, font_builtin_fonts[DEFAULT_FONT], 0, 26, config->password, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
    display_draw_qrcode(&qrcode, 64, 5, 2);

    ssd1306_load_frame_buffer(&display, display_buffer);

    free(qrcodeBytes);
    qrcode_shown = true;
}

void qrcode_hide() {
    if (!qrcode_shown)
        return;

    ssd1306_clear_screen(&display);
    ssd1306_display_on(&display, false);

    qrcode_shown = false;
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


void on_homekit_event(homekit_event_t event) {
    if (event == HOMEKIT_EVENT_PAIRING_ADDED) {
        qrcode_hide();
    } else if (event == HOMEKIT_EVENT_PAIRING_REMOVED) {
        if (!homekit_is_paired())
            sdk_system_restart();
    }
}


homekit_server_config_t config = {
    .accessories = accessories,
    .on_event = on_homekit_event,
};

void generate_random_password(char *password) {
    for (int i=0; i<10; i++) {
        password[i] = hwrand() % 10 + '0';
    }
    password[3] = password[6] = '-';
    password[10] = 0;
}

void generate_random_setup_id(char *setup_id) {
    static char chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    for (int i=0; i < 4; i++)
        setup_id[i] = chars[hwrand() % 36];
    setup_id[4] = 0;
}

char password[11];
char setup_id[5];

void user_init(void) {
    uart_set_baud(0, 115200);

    display_init();

    wifi_init();
    led_init();

    generate_random_password(password);
    generate_random_setup_id(setup_id);
    config.password = password;
    config.setupId = setup_id;

    if (!homekit_is_paired()) {
        qrcode_show(&config);
    }

    homekit_server_init(&config);
}
