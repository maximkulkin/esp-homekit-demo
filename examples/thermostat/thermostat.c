/*
 * Thermostat example
 */
#define TEMPERATURE_SENSOR_PIN 2 // GPIO2 is D4 on NodeMCU
#define HEATER_PIN 15 // GPIO15 is D8 on NodeMCU
#define TEMPERATURE_POLL_PERIOD 10000 // Temp refresh rate to milliseconds
#define TEMP_DIFF 0.5 // Set this for differential 
#define INVERT_RELAY_SWITCH 0
#define DHT_TYPE DHT_TYPE_DHT22 // If you are using DHT11 change the type to DHT_TYPE_DHT11
#define BUTTON_UP_PIN 12 // GPIO12 is D6 on NodeMCU
#define BUTTON_DOWN_PIN 13 // GPIO13 is D7 on NodeMCU
#define BUTTON_RESET_PIN 14 // GPIO14 is D5 on NodeMCU

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
#include <math.h>
#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include "wifi_config.h"
#include <button.h>
#include <dht/dht.h>
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
homekit_characteristic_t current_state = HOMEKIT_CHARACTERISTIC_(CURRENT_HEATING_COOLING_STATE, 1);
homekit_characteristic_t target_state = HOMEKIT_CHARACTERISTIC_(
    TARGET_HEATING_COOLING_STATE, 1, .valid_values = {.count = 2, .values = (uint8_t[]) { 0, 1}}, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_update)
);
homekit_characteristic_t cooling_threshold = HOMEKIT_CHARACTERISTIC_(
    COOLING_THRESHOLD_TEMPERATURE, 25, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_update)
);
homekit_characteristic_t heating_threshold = HOMEKIT_CHARACTERISTIC_(
    HEATING_THRESHOLD_TEMPERATURE, 15, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_update)
);
homekit_characteristic_t current_humidity = HOMEKIT_CHARACTERISTIC_(CURRENT_RELATIVE_HUMIDITY, 0);

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
#define DEFAULT_FONT1 FONT_FACE_TERMINUS_BOLD_12X24_ISO8859_1
#define DEFAULT_FONT2 FONT_FACE_TERMINUS_BOLD_14X28_ISO8859_1
#define DEFAULT_FONT3 FONT_FACE_TERMINUS_BOLD_16X32_ISO8859_1

static const ssd1306_t display = {
    .protocol = SSD1306_PROTO_I2C,
    .screen = SSD1306_SCREEN,
    .i2c_dev.bus = I2C_BUS,
    .i2c_dev.addr = SSD1306_I2C_ADDR_0,
    .width = DISPLAY_WIDTH,
    .height = DISPLAY_HEIGHT,
};

static uint8_t display_buffer[DISPLAY_WIDTH * DISPLAY_HEIGHT / 8];


void display_temperature_task(void *_args) {
    //printf(" TEMP %g, HUM %g, TARGET: %g\n", temperature, humidity, target_temperature.value.float_value);
    while (1) {
        float temperature = current_temperature.value.float_value;
        float humidity = current_humidity.value.float_value;
        char str[16];
        //float f = 123.456789;
        //snprintf(str, sizeof(str), "%.2f", temperature);
        
        //ssd1306_display_on(&display, true);
        
        // Display temp
        snprintf(str, sizeof(str), "%.1f", temperature);
        ssd1306_fill_rectangle(&display, display_buffer, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, OLED_COLOR_BLACK);
        ssd1306_draw_string(&display, display_buffer, font_builtin_fonts[DEFAULT_FONT2], 0, 0, str, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
        ssd1306_draw_string(&display, display_buffer, font_builtin_fonts[DEFAULT_FONT2], 64, 0, "C", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
        //ssd1306_draw_string(&display, display_buffer, font_builtin_fonts[DEFAULT_FONT2], 50, 0, "°", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
    
        // Display humidity
        snprintf(str, sizeof(str), "%.1f", humidity);
        ssd1306_draw_string(&display, display_buffer, font_builtin_fonts[DEFAULT_FONT2], 0, 24, str, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
        ssd1306_draw_string(&display, display_buffer, font_builtin_fonts[DEFAULT_FONT2], 64, 24, "%", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
    
        // Display target temp    
        snprintf(str, sizeof(str), "Target: %.1f", target_temperature.value.float_value);
        ssd1306_draw_string(&display, display_buffer, font_builtin_fonts[DEFAULT_FONT], 0, 48, str, OLED_COLOR_WHITE, OLED_COLOR_BLACK);
        ssd1306_draw_string(&display, display_buffer, font_builtin_fonts[DEFAULT_FONT], 80, 48, "C", OLED_COLOR_WHITE, OLED_COLOR_BLACK);
        //ssd1306_draw_string(&display, display_buffer, font_builtin_fonts[DEFAULT_FONT], 78, 48, "°", OLED_COLOR_WHITE, OLED_COLOR_BLACK);

        if (ssd1306_load_frame_buffer(&display, display_buffer)) {
            printf("Failed to load buffer for OLED display\n");
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}
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
    xTaskCreate(display_temperature_task, "Display", 512, NULL, 2, NULL);
}


void thermostat_identify(homekit_value_t _value) {
    printf("Thermostat identify\n");
}

void heaterOn() {
    gpio_write(HEATER_PIN, INVERT_RELAY_SWITCH ? false : true);
}


void heaterOff() {
    gpio_write(HEATER_PIN, INVERT_RELAY_SWITCH ? true : false);
}

void update_state() {
    uint8_t state = target_state.value.int_value;
    if ((state == 1 && current_temperature.value.float_value < target_temperature.value.float_value) ||
            (state == 3 && current_temperature.value.float_value < heating_threshold.value.float_value)) {
        if (current_state.value.int_value != 1) {
            current_state.value = HOMEKIT_UINT8(1);
            homekit_characteristic_notify(&current_state, current_state.value);
            heaterOn();
            
        }
    } else if ((state == 2 && current_temperature.value.float_value > target_temperature.value.float_value) ||
            (state == 3 && current_temperature.value.float_value > cooling_threshold.value.float_value)) {
        if (current_state.value.int_value != 2) {
            current_state.value = HOMEKIT_UINT8(2);
            homekit_characteristic_notify(&current_state, current_state.value);
            heaterOff();
        }
    } else {
        if (current_state.value.int_value != 0) {
            current_state.value = HOMEKIT_UINT8(0);
            homekit_characteristic_notify(&current_state, current_state.value);
            heaterOff();
        }
    }
}


void temperature_sensor_task(void *_args) {

    
    float humidity_value, temperature_value, temp_difference;
    while (1) {
        bool success = dht_read_float_data(
            DHT_TYPE, TEMPERATURE_SENSOR_PIN,
            &humidity_value, &temperature_value
        );
        if (success) {
            //printf("Got readings: temperature %g, humidity %g\n", temperature_value, humidity_value);
            current_temperature.value = HOMEKIT_FLOAT(temperature_value);
            current_humidity.value = HOMEKIT_FLOAT(humidity_value);

            homekit_characteristic_notify(&current_temperature, current_temperature.value);
            homekit_characteristic_notify(&current_humidity, current_humidity.value);
	        //display_temperature(temperature_value, humidity_value);
            temp_difference = current_temperature.value.float_value - target_temperature.value.float_value;
            //printf("temp difference: %.2f\n",fabs(temp_difference));
            if (fabs(temp_difference) > TEMP_DIFF) {
                //printf("updating state\n");
                update_state();
            }
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
            HOMEKIT_CHARACTERISTIC(MODEL, "SmartThermostat"),
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
static bool homekit_initialized = false;

void on_wifi_config_event(wifi_config_event_t event) {
    if (event == WIFI_CONFIG_CONNECTED) {
        printf("WIFI_CONFIG_CONNECTED\n");
        if (!homekit_initialized) {
            printf("Starting Homekit server...\n");
            homekit_server_init(&config);
            homekit_initialized = true;
        }
    }
}
void button_up_callback(button_event_t event, void* context) {
    // up button pressed
    printf("Button up pressed\n");
    float newTargetTemp = target_temperature.value.float_value + 0.5;
    target_temperature.value = HOMEKIT_FLOAT(newTargetTemp);
    homekit_characteristic_notify(&target_temperature, target_temperature.value);
    //display_temperature(current_temperature.value.float_value, current_humidity.value.float_value);

}

void button_down_callback(button_event_t event, void* context) {
    // down button pressed
    printf("Button down pressed\n");
    float newTargetTemp = target_temperature.value.float_value - 0.5;
    target_temperature.value = HOMEKIT_FLOAT(newTargetTemp);
    homekit_characteristic_notify(&target_temperature, target_temperature.value);
    //display_temperature(current_temperature.value.float_value, current_humidity.value.float_value);
}
void button_reset_callback(button_event_t event, void* context) {
    // down button pressed
    switch (event) {
        case button_event_single_press:
            printf("single press\n");
            break;
        case button_event_double_press:
            printf("double press\n");
            break;
        case button_event_tripple_press:
            printf("tripple press\n");
            break;
        case button_event_long_press:
            printf("long press\n");
            printf("Resetting Wifi Config\n");
            wifi_config_reset();
            printf("Resetting HomeKit Config\n");
            homekit_server_reset();
            printf("Restarting\n");
            sdk_system_restart();
            break;
    }
    
}
void gpio_init(){
    //gpio_set_pullup(TEMPERATURE_SENSOR_PIN, false, false);
    gpio_enable(HEATER_PIN, GPIO_OUTPUT);
    gpio_enable(BUTTON_DOWN_PIN, GPIO_INPUT);
    gpio_enable(BUTTON_UP_PIN, GPIO_INPUT);
    gpio_set_pullup(BUTTON_DOWN_PIN, true, true);
    gpio_set_pullup(BUTTON_UP_PIN, true, true);
    heaterOff();
}
void user_init(void) {
    uart_set_baud(0, 115200);
    gpio_init();
    wifi_config_init2("Thermostat", "mythermostat", on_wifi_config_event);
    thermostat_init();
    display_init();
    button_config_t buttonconfig = BUTTON_CONFIG(
        button_active_low,
        .long_press_time = 3000,
        .max_repeat_presses = 3,
    );
    if (button_create(BUTTON_UP_PIN, buttonconfig, button_up_callback, NULL)) {
        printf("Failed to initialize button up\n");
    }
    if (button_create(BUTTON_DOWN_PIN, buttonconfig, button_down_callback, NULL)) {
        printf("Failed to initialize button down\n");
    }
    if (button_create(BUTTON_RESET_PIN, buttonconfig, button_reset_callback, NULL)) {
        printf("Failed to initialize button resetwn\n");
    }
}

