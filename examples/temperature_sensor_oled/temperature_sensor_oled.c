#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include "wifi.h"

#include <dht/dht.h>

// OLED Stuff Start
#include <i2c/i2c.h>
#include <ssd1306/ssd1306.h>
#include <fonts/fonts.h>
#define I2C_BUS 0
#define I2C_SDA_PIN 5
#define I2C_SCL_PIN 4
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64
#define DEFAULT_FONT FONT_FACE_TERMINUS_6X12_ISO8859_1
//#define DEFAULT_FONT0 FONT_FACE_TERMINUS_BOLD_8X14_ISO8859_1
//#define DEFAULT_FONT-x FONT_FACE_TERMINUS_BOLD_10X18_ISO8859_1
//#define DEFAULT_FONT-x FONT_FACE_TERMINUS_BOLD_11X22_ISO8859_1
#define DEFAULT_FONT1 FONT_FACE_TERMINUS_BOLD_12X24_ISO8859_1
#define DEFAULT_FONT2 FONT_FACE_TERMINUS_BOLD_14X28_ISO8859_1
#define DEFAULT_FONT3 FONT_FACE_TERMINUS_BOLD_16X32_ISO8859_1
// OLED Stuff Stop

#ifndef SENSOR_PIN
#error SENSOR_PIN is not specified
#endif

//OLED START
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
    printf("Starting display init...");
    i2c_init(I2C_BUS, I2C_SCL_PIN, I2C_SDA_PIN, I2C_FREQ_400K);
    if (ssd1306_init(&display)) {
        printf("Failed to initialize OLED display\n");
        return;
    }
    ssd1306_set_whole_display_lighting(&display, false);
    ssd1306_set_scan_direction_fwd(&display, false);
    ssd1306_set_segment_remapping_enabled(&display, true);
    ssd1306_display_on(&display, true);
}

void display_temperature(float temperature, float humidity) {
    //printf(" TEMP %g, HUM %g\n", temperature, humidity);

    char str[16];
    //float f = 123.456789;
    //snprintf(str, sizeof(str), "%.2f", temperature);
    snprintf(str, sizeof(str), "%.1f", temperature);

    //ssd1306_display_on(&display, true);

    ssd1306_fill_rectangle(&display, display_buffer, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, OLED_COLOR_BLACK);
    ssd1306_draw_string(&display, display_buffer, font_builtin_fonts[DEFAULT_FONT3], 0, 0, str, OLED_COLOR_WHITE, OLED_COLOR_BLACK);

    //ssd1306_draw_string(&display, display_buffer, font_builtin_fonts[DEFAULT_FONT3], 0, 0, "2130", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
    ssd1306_draw_string(&display, display_buffer, font_builtin_fonts[DEFAULT_FONT2], 90, 0, "C", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
    ssd1306_draw_string(&display, display_buffer, font_builtin_fonts[DEFAULT_FONT1], 78, 0, "o", OLED_COLOR_WHITE, OLED_COLOR_BLACK);

    //ssd1306_draw_string(&display, display_buffer, font_builtin_fonts[DEFAULT_FONT], 0, 0, "Temperature", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
    snprintf(str, sizeof(str), "%.1f", humidity);
    ssd1306_draw_string(&display, display_buffer, font_builtin_fonts[DEFAULT_FONT3], 0, 32, str, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
    ssd1306_draw_string(&display, display_buffer, font_builtin_fonts[DEFAULT_FONT2], 90, 34, "%", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
    //ssd1306_draw_string(&display, display_buffer, font_builtin_fonts[DEFAULT_FONT3], 2, 2, "0", OLED_COLOR_WHITE, OLED_COLOR_BLACK);


    ssd1306_load_frame_buffer(&display, display_buffer);


}
//OLED STOP

static void wifi_init() {
    struct sdk_station_config wifi_config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASSWORD,
    };

    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&wifi_config);
    sdk_wifi_station_connect();
}


void temperature_sensor_identify(homekit_value_t _value) {
    printf("Temperature sensor identify\n");
}

homekit_characteristic_t temperature = HOMEKIT_CHARACTERISTIC_(CURRENT_TEMPERATURE, 0);
homekit_characteristic_t humidity    = HOMEKIT_CHARACTERISTIC_(CURRENT_RELATIVE_HUMIDITY, 0);


void temperature_sensor_task(void *_args) {
    gpio_set_pullup(SENSOR_PIN, false, false);

    float humidity_value, temperature_value;
    while (1) {
        bool success = dht_read_float_data(
            DHT_TYPE_DHT22, SENSOR_PIN,
            &humidity_value, &temperature_value
        );
        if (success) {
            temperature.value.float_value = temperature_value;
            humidity.value.float_value = humidity_value;

            homekit_characteristic_notify(&temperature, HOMEKIT_FLOAT(temperature_value));
            homekit_characteristic_notify(&humidity, HOMEKIT_FLOAT(humidity_value));
            //OLED START
	    display_temperature(temperature_value, humidity_value);
            //OLED STOP
        } else {
            printf("Couldnt read data from sensor\n");
        }

        vTaskDelay(10000 / portTICK_PERIOD_MS);
    }
}

void temperature_sensor_init() {
    xTaskCreate(temperature_sensor_task, "Temperatore Sensor", 512, NULL, 2, NULL);
}


homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_thermostat, .services=(homekit_service_t*[]) {
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Temperature Sensor"),
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "HaPK"),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "0012345"),
            HOMEKIT_CHARACTERISTIC(MODEL, "MyTemperatureSensor"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, temperature_sensor_identify),
            NULL
        }),
        HOMEKIT_SERVICE(TEMPERATURE_SENSOR, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Temperature Sensor"),
            &temperature,
            NULL
        }),
        HOMEKIT_SERVICE(HUMIDITY_SENSOR, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Humidity Sensor"),
            &humidity,
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
    display_init();
    wifi_init();
    temperature_sensor_init();
    homekit_server_init(&config);
}


