#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>
#include <assert.h>
#include <etstimer.h>
#include <esplibs/libmain.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include "wifi.h"
#include "contact_sensor.h"

// Possible values for characteristic CURRENT_DOOR_STATE:
#define HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPEN 0
#define HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSED 1
#define HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPENING 2
#define HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSING 3
#define HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_STOPPED 4
#define HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_UNKNOWN 255

#define HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_OPEN 0
#define HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_CLOSED 1
#define HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_UNKNOWN 255

#define OPEN_CLOSE_DURATION 22

const char *state_description(uint8_t state) {
    const char* description = "unknown";
    switch (state) {
        case HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPEN: description = "open"; break;
        case HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPENING: description = "opening"; break;
        case HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSED: description = "closed"; break;
        case HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSING: description = "closing"; break;
        case HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_STOPPED: description = "stopped"; break;
        default: ;
    }
    return description;
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

// Declare functions:

homekit_value_t gdo_target_state_get();
void gdo_target_state_set(homekit_value_t new_value);
homekit_value_t gdo_current_state_get();
homekit_value_t gdo_obstruction_get();
void identify(homekit_value_t _value);

// Declare global variables:

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_garage, .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Garagentor"),
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "ObjP"),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "237A2BAB119E"),
            HOMEKIT_CHARACTERISTIC(MODEL, "GDO"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.2"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, identify),
            NULL
        }),
        HOMEKIT_SERVICE(GARAGE_DOOR_OPENER, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Tor"),
            HOMEKIT_CHARACTERISTIC(
                CURRENT_DOOR_STATE, HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSED,
                .getter=gdo_current_state_get,
                .setter=NULL
            ),
            HOMEKIT_CHARACTERISTIC(
                TARGET_DOOR_STATE, HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_CLOSED,
                .getter=gdo_target_state_get,
                .setter=gdo_target_state_set
            ),
            HOMEKIT_CHARACTERISTIC(
                OBSTRUCTION_DETECTED, HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_CLOSED,
                .getter=gdo_obstruction_get,
                .setter=NULL
            ),
            NULL
        }),
        NULL
    }),
    NULL
};

bool relay_on = false;
uint8_t current_door_state = HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_UNKNOWN;
ETSTimer update_timer; // used for delayed updating from contact sensor



void relay_write(bool on) {
    gpio_write(RELAY_PIN, on ? 0 : 1);
}

void relay_init() {
    gpio_enable(RELAY_PIN, GPIO_OUTPUT);
    relay_write(relay_on);
}

void identify_task(void *_args) {
    // 1. move the door, 2. stop it, 3. move it back:
    for (int i=0; i<3; i++) {
            relay_write(true);
            vTaskDelay(500 / portTICK_PERIOD_MS);
            relay_write(false);
            vTaskDelay(3500 / portTICK_PERIOD_MS);
    }

    relay_write(false);

    vTaskDelete(NULL);
}

void identify(homekit_value_t _value) {
    printf("GDO identify\n");
    xTaskCreate(identify_task, "GDO identify", 128, NULL, 2, NULL);
}

homekit_value_t relay_on_get() {
    return HOMEKIT_BOOL(relay_on);
}

void relay_on_set(homekit_value_t value) {
    if (value.format != homekit_format_bool) {
        printf("Invalid value format: %d\n", value.format);
        return;
    }

    relay_on = value.bool_value;
    relay_write(relay_on);
}

homekit_value_t gdo_obstruction_get() {
    return HOMEKIT_BOOL(false);
}

void gdo_current_state_notify_homekit() {

    homekit_value_t new_value = HOMEKIT_UINT8(current_door_state);
    printf("Notifying homekit that current door state is now '%s'\n", state_description(current_door_state));

    // Find the current door state characteristic c:
    homekit_accessory_t *accessory = accessories[0];
    homekit_service_t *service = accessory->services[1];
    homekit_characteristic_t *c = service->characteristics[1];

    assert(c);

    printf("Notifying changed '%s'\n", c->description);
    homekit_characteristic_notify(c, new_value);
}

void gdo_target_state_notify_homekit() {

    homekit_value_t new_value = gdo_target_state_get();
    printf("Notifying homekit that target door state is now '%s'\n", state_description(new_value.int_value));

    // Find the target door state characteristic c:
    homekit_accessory_t *accessory = accessories[0];
    homekit_service_t *service = accessory->services[1];
    homekit_characteristic_t *c = service->characteristics[2];

    assert(c);

    printf("Notifying changed '%s'\n", c->description);
    homekit_characteristic_notify(c, new_value);
}

void current_state_set(uint8_t new_state) {
    if (current_door_state != new_state) {
        current_door_state = new_state;
        gdo_target_state_notify_homekit();
        gdo_current_state_notify_homekit();
    }
}

void current_door_state_update_from_sensor() {
    contact_sensor_state_t sensor_state = contact_sensor_state_get(REED_PIN);

    switch (sensor_state) {
        case CONTACT_CLOSED:
            current_state_set(HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPEN);
            break;
        case CONTACT_OPEN:
            current_state_set(HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSED);
            break;
        default:
            printf("Unknown contact sensor event: %d\n", sensor_state);
    }
}

homekit_value_t gdo_current_state_get() {
    if (current_door_state == HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_UNKNOWN) {
	current_door_state_update_from_sensor();
    }
    printf("returning current door state '%s'.\n", state_description(current_door_state));

    return HOMEKIT_UINT8(current_door_state);
}

homekit_value_t gdo_target_state_get() {
    uint8_t result = gdo_current_state_get().int_value;
    if (result == HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPENING) {
	result = HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_OPEN;
    } else if (result == HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSING) {
        result = HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_CLOSED;  
    }

    printf("Returning target door state '%s'.\n", state_description(result));

    return HOMEKIT_UINT8(result);
}

/**
 * Called (indirectly) from the interrupt handler to notify the client of a state change.
 **/
void contact_sensor_state_changed(uint8_t gpio, contact_sensor_state_t state) {

    printf("contact sensor state '%s'.\n", state == CONTACT_OPEN ? "open" : "closed");

    if (current_door_state == HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPENING ||
        current_door_state == HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSING) {
	// Ignore the event - the state will be updated after the time expired!
        printf("contact_sensor_state_changed() ignored during opening or closing.\n");
	return;
    }
    current_door_state_update_from_sensor();
}


void gdo_target_state_set(homekit_value_t new_value) {

    if (new_value.format != homekit_format_uint8) {
        printf("Invalid value format: %d\n", new_value.format);
        return;
    }

    if (current_door_state != HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPEN &&
        current_door_state != HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSED) {
        printf("gdo_target_state_set() ignored: current state not open or closed (%s).\n", state_description(current_door_state));
	return;
    }

    if (current_door_state == new_value.int_value) {
        printf("gdo_target_state_set() ignored: new target state == current state (%s)\n", state_description(current_door_state));
        return;
    }

    // Toggle the garage door by toggling the relay connected to the GPIO (on - off):
    // Turn ON GPIO:
    relay_write(true);
    // Wait for some time:
    vTaskDelay(400 / portTICK_PERIOD_MS);
    // Turn OFF GPIO:
    relay_write(false);
    if (current_door_state == HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSED) {
        current_state_set(HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPENING);
    } else {
        current_state_set(HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSING);
    }
    // Wait for the garage door to open / close,
    // then update current_door_state from sensor:
    sdk_os_timer_arm(&update_timer, OPEN_CLOSE_DURATION * 1000, false);
}


static void timer_callback(void *arg) {

    printf("Timer fired. Updating state from sensor.\n");
    sdk_os_timer_disarm(&update_timer);
    current_door_state_update_from_sensor();
}


homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111"
};

void user_init(void) {
    uart_set_baud(0, 9600);

    wifi_init();
    relay_init();

    // Initialize Timer:
    sdk_os_timer_disarm(&update_timer);
    sdk_os_timer_setfn(&update_timer, timer_callback, NULL);


    printf("Using Sensor at GPIO%d.\n", REED_PIN);
    if (contact_sensor_create(REED_PIN, contact_sensor_state_changed)) {
        printf("Failed to initialize door\n");
    }

    homekit_server_init(&config);

}
