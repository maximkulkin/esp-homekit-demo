#include <stdio.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>

#include <button.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include "wifi.h"


void on_wifi_ready();

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && (event_id == WIFI_EVENT_STA_START || event_id == WIFI_EVENT_STA_DISCONNECTED)) {
        printf("STA start\n");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        printf("WiFI ready\n");
        on_wifi_ready();
    }
}

static void wifi_init() {
    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

const int button_gpio = 0;
const int led_gpio = 2;


void led_write(bool on) {
    gpio_set_level(led_gpio, on ? 1 : 0);
}


void button_identify_task(void *_args) {
    for (int i=0; i<3; i++) {
        for (int j=0; j<2; j++) {
            led_write(true);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            led_write(false);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

        vTaskDelay(250 / portTICK_PERIOD_MS);
    }

    led_write(false);

    vTaskDelete(NULL);
}

void button_identify(homekit_value_t _value) {
    printf("LED identify\n");
    xTaskCreate(button_identify_task, "Button identify", 512, NULL, 2, NULL);
}

homekit_characteristic_t button_event = HOMEKIT_CHARACTERISTIC_(PROGRAMMABLE_SWITCH_EVENT, 0);


void button_callback(button_event_t event, void *context) {
    switch (event) {
        case button_event_single_press:
            printf("single press\n");
            homekit_characteristic_notify(&button_event, HOMEKIT_UINT8(0));
            break;
        case button_event_double_press:
            printf("double press\n");
            homekit_characteristic_notify(&button_event, HOMEKIT_UINT8(1));
            break;
        case button_event_long_press:
            printf("long press\n");
            homekit_characteristic_notify(&button_event, HOMEKIT_UINT8(2));
            break;
        default:
            printf("unknown button event: %d\n", event);
    }
}


homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(
        .id=1,
        .category=homekit_accessory_category_programmable_switch,
        .services=(homekit_service_t*[]) {
            HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
                HOMEKIT_CHARACTERISTIC(NAME, "Button"),
                HOMEKIT_CHARACTERISTIC(MANUFACTURER, "HaPK"),
                HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "0012345"),
                HOMEKIT_CHARACTERISTIC(MODEL, "MyButton"),
                HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1"),
                HOMEKIT_CHARACTERISTIC(IDENTIFY, button_identify),
                NULL
            }),
            HOMEKIT_SERVICE(STATELESS_PROGRAMMABLE_SWITCH, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
                HOMEKIT_CHARACTERISTIC(NAME, "Button"),
                &button_event,
                NULL
            }),
            NULL
        },
    ),
    NULL
};


homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111"
};


void on_wifi_ready() {
    homekit_server_init(&config);
}

void app_main(void) {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    wifi_init();

    button_config_t button_config = BUTTON_CONFIG(
        button_active_low, 
        .max_repeat_presses=2,
        .long_press_time=1000,
    );
    if (button_create(button_gpio, button_config, button_callback, NULL)) {
        printf("Failed to initialize button\n");
    }
}
