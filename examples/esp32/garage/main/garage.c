#include <stdio.h>
#include <driver/gpio.h>
#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <sys/time.h>

#include "wifi.h"


// Possible values for characteristic CURRENT_DOOR_STATE:
#define HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPEN 0
#define HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSED 1
#define HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPENING 2
#define HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSING 3
#define HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_STOPPED 4

#define HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_OPEN 0
#define HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_CLOSED 1

#define USR_PIN_TRIGGER 26
#define USR_PIN_ECHO    25
#define PING_TIMEOUT  6000

#define DISTANCE_CLOSED 190 // greater than
#define DISTANCE_OPEN 20 // smaller than
#define DISTANCE_CHANGE 2 // minimal change
#define TIME_STUCK 5 // minimal change




// MARK: -

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

  printf("gdo_target_on_set %d\n", new_value.int_value);
  if (new_value.format != homekit_format_uint8) {
    printf("Invalid value format: %d\n", new_value.format);
    return;
  }
  //target_door_state = new_value.int_value;
  gdo_current_state_notify_homekit();
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
  homekit_value_t new_value2 = HOMEKIT_UINT8(target_door_state);
  // Find the current door state characteristic c:
  homekit_accessory_t *accessory = accessories[0];
  homekit_service_t *service = accessory->services[1];

  homekit_characteristic_t *c2 = service->characteristics[2];
  assert(c2);
  homekit_characteristic_notify(c2, new_value2);

  homekit_characteristic_t *c = service->characteristics[1];
  assert(c);
  //printf("Notifying '%s'\n", c->description);
  homekit_characteristic_notify(c, new_value);


}

homekit_server_config_t config = {
  .accessories = accessories,
  .password = "123-45-678"
};

// MARK: - Ultrasonic

int distance = 0;
int preDist = 0;
int64_t lastMoveTime_ms = 0;
int64_t lastNotifyTime_ms = 0;
void ultrasonic_init() {
  gpio_set_direction(USR_PIN_TRIGGER, GPIO_MODE_OUTPUT);
  gpio_set_direction(USR_PIN_ECHO, GPIO_MODE_INPUT);
  gpio_set_level(USR_PIN_TRIGGER, 0);
}

void update_doorstate() {

  bool updateHK = false;
  int64_t nowTime_ms = esp_timer_get_time()/1000;

  if (distance < DISTANCE_OPEN) {
    preDist = distance;
    if (current_door_state != HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPEN) {
      printf("Dist: %d\n", distance);
      printf("Door is now open\n");
      current_door_state = HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPEN;
      target_door_state = HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_OPEN;
      printf("Door update S: %d  T: %d\n", current_door_state, target_door_state);
      updateHK = true;
    }
  } else if (distance > DISTANCE_CLOSED) {
    preDist = distance;
    if (current_door_state != HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSED) {
      printf("Dist: %d\n", distance);
      printf("Door is now closed\n");
      current_door_state = HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSED;
      target_door_state = HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_CLOSED;
      printf("Door update S: %d  T: %d\n", current_door_state, target_door_state);
      updateHK = true;
    }
  } else {
    int diff = 0;
    int isMoving = 0; // 0- no, 1 open, 2- closing
    if (distance > preDist) {
      diff = distance - preDist;
      isMoving = 2;
    } else if (distance < preDist) {
      diff = preDist - distance;
      isMoving = 1;
    }
    if (diff < DISTANCE_CHANGE) {
      isMoving = 0;
    } else {
      // set move time
      lastMoveTime_ms = esp_timer_get_time()/1000;
    }

    if (isMoving == 1) {
      printf("Dist: %d\n", distance);
      if (current_door_state != HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPENING) {
        printf("Door starts opening\n");
        current_door_state = HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPENING;
        updateHK = true;
      }
      if (target_door_state != HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_OPEN) {
        target_door_state = HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_OPEN;
        updateHK = true;
      }
    } else if (isMoving == 2) {
      printf("Dist: %d\n", distance);
      if (current_door_state != HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSING) {
          printf("Door starts closing\n");
        current_door_state = HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSING;
        updateHK = true;
      }
      if (target_door_state != HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_CLOSED) {
        target_door_state = HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_CLOSED;
        updateHK = true;
      }
    } else if (isMoving == 0) { // check stuck
      int64_t diffTime = nowTime_ms - lastMoveTime_ms;
      if (diffTime > (TIME_STUCK*1000)) {
        if (current_door_state != HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_STOPPED) {
          current_door_state = HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_STOPPED;
          printf("Door is now stopped\n");
          updateHK = true;
        }
      }
    }
  }
  if (!updateHK) {
    int64_t diffTime = nowTime_ms - lastNotifyTime_ms;
    if (diffTime > 10000) {
      updateHK = true;
    }
  }

  if (updateHK) {
    printf("Door update S: %d  T: %d\n", current_door_state, target_door_state);
    gdo_current_state_notify_homekit();
   lastNotifyTime_ms = esp_timer_get_time()/1000;
  }
}

void ultrasonic_read() {

  gpio_set_level(USR_PIN_TRIGGER, 0);
  ets_delay_us(2);
  gpio_set_level(USR_PIN_TRIGGER, 1);
  ets_delay_us(20);
  gpio_set_level(USR_PIN_TRIGGER, 0);
  if (gpio_get_level(USR_PIN_ECHO)) {
    printf("Error 1 : (prev ping)\n");
    return;
  }

  int64_t timeStart = esp_timer_get_time();
  int64_t timeEnd = timeStart;
  bool exitLoop = false;
  bool firstUp = true;
  while (!exitLoop) {
    timeEnd = esp_timer_get_time();
    int lvl = gpio_get_level(USR_PIN_ECHO);
    if (!firstUp && lvl == 0) {
      exitLoop = true;
    }
    if (lvl == 1) {
      if (firstUp) {
        timeStart = timeEnd;
        firstUp = false;
      }
    }
    int64_t diff =  timeEnd - timeStart;
    if (diff > 30000) {
      exitLoop = true;
      timeEnd = 0;
      timeStart = 0;
      //printf("Error 2 : (timeout)\n");
    }
  }

  int64_t timeDur = timeEnd - timeStart;
  if (timeDur > 100) {
    int dist = timeDur/58;
    distance = dist;
    //printf("Start: %lld\n", timeStart);
    //printf("Stop: %lld\n", timeEnd);
    //printf("Duration: %lld\n", timeDur);
    //printf("Dist: %d cm\n", dist);
    update_doorstate();
  }
}


// MARK: - WiFi

void on_wifi_ready() {
  homekit_server_init(&config);
}

static void periodic_timer_callback(void* arg) {
  ultrasonic_read();
}

ETSTimer update_timer; // used for delayed updating from contact sensor
void app_main(void) {
  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK( ret );
  wifi_init();
  ultrasonic_init();

  const esp_timer_create_args_t periodic_timer_args = {
    .callback = &periodic_timer_callback,
    /* name is optional, but may help identify the timer when debugging */
    .name = "periodic"
  };

  esp_timer_handle_t periodic_timer;
  ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 1000000));
}
