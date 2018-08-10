/*
* This is an example of an rgb ws2812_i2s led strip
*
* NOTE:
*    1) the ws2812_i2s library uses hardware I2S so output pin is GPIO3 and cannot be changed.
*    2) on some ESP8266 such as the Wemos D1 mini, GPIO3 is the same pin used for serial comms.
*    3) Toggle button can be placed on GPIO0. Click to toggle on/off. Hold to reset config.

* Debugging printf statements are disabled below because of note (2) - you can uncomment
* them if your hardware supports serial comms that do not conflict with I2S on GPIO3.
*
* Contributed March 2018 by https://github.com/Dave1001
*/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <math.h>
#include "button.h"

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>
#include "ws2812_i2s/ws2812_i2s.h"

#define LED_ON 0                // this is the value to write to GPIO for led on (0 = GPIO low)
#define LED_INBUILT_GPIO 2      // this is the onboard LED used to show on/off only
#define LED_COUNT 16            // this is the number of WS2812B leds on the strip
#define LED_RGB_SCALE 255       // this is the scaling factor used for color conversion

// Global variables
float led_hue = 0;              // hue is scaled 0 to 360
float led_saturation = 59;      // saturation is scaled 0 to 100
float led_brightness = 100;     // brightness is scaled 0 to 100
bool led_on = false;            // on is boolean on or off
const int button_gpio = 0;      // Button GPIO pin - Click On/Off, 30s Hold Reset
ws2812_pixel_t pixels[LED_COUNT];

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

void led_string_fill(ws2812_pixel_t rgb) {

  // write out the new color to each pixel
  for (int i = 0; i < LED_COUNT; i++) {
    pixels[i] = rgb;
  }
  ws2812_i2s_update(pixels, PIXEL_RGB);
}

void led_string_set(void) {
  ws2812_pixel_t rgb = { { 0, 0, 0, 0 } };

  if (led_on) {
    // convert HSI to RGBW
    hsi2rgb(led_hue, led_saturation, led_brightness, &rgb);
    //printf("h=%d,s=%d,b=%d => ", (int)led_hue, (int)led_saturation, (int)led_brightness);
    //printf("r=%d,g=%d,b=%d,w=%d\n", rgbw.red, rgbw.green, rgbw.blue, rgbw.white);

    // set the inbuilt led
    gpio_write(LED_INBUILT_GPIO, LED_ON);
  }
  else {
    // printf("off\n");
    gpio_write(LED_INBUILT_GPIO, 1 - LED_ON);
  }

  // write out the new color
  led_string_fill(rgb);
}

void led_init() {
  // initialise the onboard led as a secondary indicator (handy for testing)
  gpio_enable(LED_INBUILT_GPIO, GPIO_OUTPUT);

  // initialise the LED strip
  ws2812_i2s_init(LED_COUNT, PIXEL_RGB);

  // set the initial state
  led_string_set();
}

void led_identify_task(void *_args) {
  const ws2812_pixel_t COLOR_PINK = { { 255, 0, 127, 0 } };
  const ws2812_pixel_t COLOR_BLACK = { { 0, 0, 0, 0 } };

  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      gpio_write(LED_INBUILT_GPIO, LED_ON);
      led_string_fill(COLOR_PINK);
      vTaskDelay(100 / portTICK_PERIOD_MS);
      gpio_write(LED_INBUILT_GPIO, 1 - LED_ON);
      led_string_fill(COLOR_BLACK);
      vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    vTaskDelay(250 / portTICK_PERIOD_MS);
  }

  led_string_set();
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
  led_string_set();
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
  led_string_set();
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
  led_string_set();
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
  led_string_set();
}

void reset_configuration_task() {
  //Flash the LED first before we start the reset
  const ws2812_pixel_t COLOR_PINK = { { 255, 0, 127, 0 } };
  const ws2812_pixel_t COLOR_BLACK = { { 0, 0, 0, 0 } };
  for (int i=0; i<3; i++) {
    gpio_write(LED_INBUILT_GPIO, LED_ON);
    led_string_fill(COLOR_PINK);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    gpio_write(LED_INBUILT_GPIO, 1 - LED_ON);
    led_string_fill(COLOR_BLACK);
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

homekit_characteristic_t lightbulb_on = HOMEKIT_CHARACTERISTIC_(ON, false, .getter=led_on_get, .setter=led_on_set);

void button_callback(uint8_t gpio, button_event_t event) {
  switch (event) {
    case button_event_single_press:
      printf("GPIO %2d - Toggling light\n", gpio);
      lightbulb_on.value.bool_value = !lightbulb_on.value.bool_value;
      led_on = lightbulb_on.value.bool_value;
      led_string_set();
      homekit_characteristic_notify(&lightbulb_on, lightbulb_on.value);
      break;
    case button_event_long_press:
      printf("GPIO %2d - Reset Triggered\n", gpio);
      reset_configuration();
      break;
    default:
      printf("unknown button event: %d\n", event);
  }
}

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, "Sample LED Strip");
homekit_characteristic_t button_event = HOMEKIT_CHARACTERISTIC_(PROGRAMMABLE_SWITCH_EVENT, 0);
homekit_characteristic_t ne = HOMEKIT_CHARACTERISTIC_(MANUFACTURER, NULL);


static void get_ip(void){
  static char ip[16];
  ip[0] = 0;
  struct ip_info ipinfo;
  (void)
  sdk_wifi_get_ip_info(STATION_IF, &ipinfo);
  snprintf(ip, sizeof(ip), IPSTR, IP2STR(&ipinfo.ip));
  ne.value.string_value = strdup(ip);
}

homekit_accessory_t *accessories[] = {
  HOMEKIT_ACCESSORY(.id = 1, .category = homekit_accessory_category_lightbulb, .services = (homekit_service_t*[]) {
    HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics = (homekit_characteristic_t*[]) {
      &name,
      &ne,
      HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "037A2BABF19D"),
      HOMEKIT_CHARACTERISTIC(MODEL, "LEDStrip"),
      HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1"),
      HOMEKIT_CHARACTERISTIC(IDENTIFY, led_identify),
      NULL
    }),
    HOMEKIT_SERVICE(LIGHTBULB, .primary = true, .characteristics = (homekit_characteristic_t*[]) {
      HOMEKIT_CHARACTERISTIC(NAME, "Sample LED Strip"),
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
    NULL
  }),
  NULL
};

homekit_server_config_t config = {
  .accessories = accessories,
  .password = "718-92-671"
};

void on_wifi_ready() {
  get_ip();
  homekit_server_init(&config);
}

void user_init(void) {
  // uart_set_baud(0, 115200);

  // This example shows how to use same firmware for multiple similar accessories
  // without name conflicts. It uses the last 3 bytes of accessory's MAC address as
  // accessory name suffix.
  uint8_t macaddr[6];
  sdk_wifi_get_macaddr(STATION_IF, macaddr);
  int name_len = snprintf(NULL, 0, "LED Lamp-%02X%02X%02X", macaddr[3], macaddr[4], macaddr[5]);
  char *name_value = malloc(name_len + 1);
  snprintf(name_value, name_len + 1, "LED Lamp-%02X%02X%02X", macaddr[3], macaddr[4], macaddr[5]);
  name.value = HOMEKIT_STRING(name_value);

  wifi_config_init("LED Lamp-", NULL, on_wifi_ready);
  led_init();

  if (button_create(button_gpio, 0, 4000, button_callback)) {
    printf("Failed to initialize button\n");
  }
}
