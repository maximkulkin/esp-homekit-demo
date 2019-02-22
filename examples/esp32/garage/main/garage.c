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

// Possible values for characteristic CURRENT_DOOR_STATE:
#define HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPEN 0
#define HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSED 1
#define HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPENING 2
#define HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSING 3
#define HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_STOPPED 4

#define HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_OPEN 0
#define HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_CLOSED 1


void on_wifi_ready();
void gdo_current_state_notify_homekit();

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


// MARK: - HomeKit Stuff

uint8_t current_door_state = HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSED;
uint8_t target_door_state = HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_CLOSED;


void gdo_simulation_task() {
  printf("gdo_simulation_taskSTART\n");
  int doIt = 0;
  if  (target_door_state == HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_OPEN ) {
    if (current_door_state == HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSED || current_door_state == HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSING) {
       if (current_door_state == HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSED) {
         doIt = 1;
       }
      current_door_state = HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPENING;
    }
  } else if (target_door_state == HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_CLOSED) {
    if (current_door_state == HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPEN || current_door_state == HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPENING) {
      if (current_door_state == HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPEN) {
        doIt = 1;
      }
      current_door_state = HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSING;
    }
  }
  vTaskDelay(100 / portTICK_PERIOD_MS);
  printf("gdo_simulation_task 1.\n");
  if (doIt == 1) {
      vTaskDelay(5000 / portTICK_PERIOD_MS);
      printf("gdo_simulation_task 2.\n");

      if (target_door_state == HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_CLOSED) {
          current_door_state = HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSED;
      } else if (target_door_state == HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_OPEN) {
          current_door_state = HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPEN;
      }
      printf("gdo_simulation_task 3.\n");

  }
  vTaskDelay(100 / portTICK_PERIOD_MS);

  gdo_current_state_notify_homekit();

  printf("gdo_simulation_task END.\n");
}

// MARK: Accessory and callbacks

void gdo_identify(homekit_value_t _value) {
    printf("GDO identify\n");
}

homekit_value_t gdo_current_on_get() {
    printf("gdo_current_on_get\n");
    return HOMEKIT_UINT8(current_door_state);
}

homekit_value_t gdo_target_on_get() {
    printf("gdo_target_on_get\n");
    return HOMEKIT_UINT8(current_door_state);
}

void gdo_target_on_set(homekit_value_t new_value) {
    printf("gdo_target_on_set\n");
    if (new_value.format != homekit_format_uint8) {
        printf("Invalid value format: %d\n", new_value.format);
        return;
    }
    target_door_state = new_value.int_value;
    gdo_simulation_task();
}

homekit_value_t gdo_obstruction_on_get() {
    printf("gdo_obstruction_on_get\n");
    return HOMEKIT_BOOL(false);
}

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_garage, .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Garagentor"),
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "ObjP"),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "237A2BAB119D"),
            HOMEKIT_CHARACTERISTIC(MODEL, "GDO"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, gdo_identify),
            NULL
        }),
        HOMEKIT_SERVICE(GARAGE_DOOR_OPENER, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Tor"),
            HOMEKIT_CHARACTERISTIC(
                CURRENT_DOOR_STATE, HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSED,
                .getter=gdo_current_on_get,
                .setter=NULL
            ),
            HOMEKIT_CHARACTERISTIC(
                TARGET_DOOR_STATE, HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_CLOSED,
                .getter=gdo_target_on_get,
                .setter=gdo_target_on_set
            ),
            HOMEKIT_CHARACTERISTIC(
                OBSTRUCTION_DETECTED, HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_CLOSED,
                .getter=gdo_obstruction_on_get,
                .setter=NULL
            ),
            NULL
        }),
        NULL
    }),
    NULL
};

void gdo_current_state_notify_homekit() {

    homekit_value_t new_value = HOMEKIT_UINT8(current_door_state);
    // Find the current door state characteristic c:
    homekit_accessory_t *accessory = accessories[0];
    homekit_service_t *service = accessory->services[1];
    homekit_characteristic_t *c = service->characteristics[1];
    assert(c);
    //printf("Notifying '%s'\n", c->description);
    homekit_characteristic_notify(c, new_value);
}

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "123-45-678"
};

// MARK: - WiFi

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
}
