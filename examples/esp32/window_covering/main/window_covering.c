#include <stdio.h>
#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include "wifi.h"

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

#define POSITION_OPEN 100
#define POSITION_CLOSED 0
#define POSITION_STATE_CLOSING 0
#define POSITION_STATE_OPENING 1
#define POSITION_STATE_STOPPED 2

TaskHandle_t updateStateTask;
homekit_characteristic_t current_position;
homekit_characteristic_t target_position;
homekit_characteristic_t position_state;
homekit_accessory_t *accessories[];

void on_wifi_ready();

esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
        case SYSTEM_EVENT_STA_START:
            printf("STA start\n");
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            printf("WiFI ready\n");
            on_wifi_ready();
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            printf("STA disconnected\n");
            esp_wifi_connect();
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void wifi_init() {
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));

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

void update_state() {
    while (true) {
        uint8_t position = current_position.value.int_value;
        int8_t direction = position_state.value.int_value == POSITION_STATE_OPENING ? 1 : -1;
        int16_t newPosition = position + direction;

        printf("position %u, target %u\n", newPosition, target_position.value.int_value);

        current_position.value.int_value = newPosition;
        homekit_characteristic_notify(&current_position, current_position.value);

        if (newPosition == target_position.value.int_value) {
            printf("reached destination %u\n", newPosition);
            position_state.value.int_value = POSITION_STATE_STOPPED;
            homekit_characteristic_notify(&position_state, position_state.value);
            vTaskSuspend(updateStateTask);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void update_state_init() {
    xTaskCreate(update_state, "UpdateState", 2048, NULL, tskIDLE_PRIORITY, &updateStateTask);
    vTaskSuspend(updateStateTask);
}

void window_covering_identify(homekit_value_t _value) {
    printf("Curtain identify\n");
}

void on_update_target_position(homekit_characteristic_t *ch, homekit_value_t value, void *context);

homekit_characteristic_t current_position = {
    HOMEKIT_DECLARE_CHARACTERISTIC_CURRENT_POSITION(POSITION_CLOSED)
};

homekit_characteristic_t target_position = {
    HOMEKIT_DECLARE_CHARACTERISTIC_TARGET_POSITION(POSITION_CLOSED, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_update_target_position))
};

homekit_characteristic_t position_state = {
    HOMEKIT_DECLARE_CHARACTERISTIC_POSITION_STATE(POSITION_STATE_STOPPED)
};

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_window_covering, .services=(homekit_service_t*[]) {
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Window blind"),
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "MNK"),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "001"),
            HOMEKIT_CHARACTERISTIC(MODEL, "MyCurtain"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, window_covering_identify),
            NULL
        }),
        HOMEKIT_SERVICE(WINDOW_COVERING, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Window blind"),
            &current_position,
            &target_position,
            &position_state,
            NULL
        }),
        NULL
    }),
    NULL
};

void on_update_target_position(homekit_characteristic_t *ch, homekit_value_t value, void *context) {
    printf("Update target position to: %u\n", target_position.value.int_value);

    if (target_position.value.int_value == current_position.value.int_value) {
        printf("Current position equal to target. Stopping.\n");
        position_state.value.int_value = POSITION_STATE_STOPPED;
        homekit_characteristic_notify(&position_state, position_state.value);
        vTaskSuspend(updateStateTask);
    } else {
        position_state.value.int_value = target_position.value.int_value > current_position.value.int_value
            ? POSITION_STATE_OPENING
            : POSITION_STATE_CLOSING;

        homekit_characteristic_notify(&position_state, position_state.value);
        vTaskResume(updateStateTask);
    }
}

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
    update_state_init();
    printf("init complete\n");
}
