#include <stdio.h>
#include <string.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_system.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <etstimer.h>
#include <esplibs/libmain.h>
#include <FreeRTOS.h>
#include <task.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include "wifi.h"


#define MAX_SERVICES 10

static void wifi_init() {
    struct sdk_station_config wifi_config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASSWORD,
    };

    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&wifi_config);
    sdk_wifi_station_connect();
}

const uint8_t pump_gpio = 16;

const uint8_t valve_gpios[1] = {
    4
};
const size_t valve_count = sizeof(valve_gpios) / sizeof(*valve_gpios);

homekit_accessory_t *accessories[2];

bool task_running;
TaskHandle_t gate_task;

typedef struct {
    int number;
    uint8_t gpio;
    homekit_characteristic_t* active;
    homekit_characteristic_t* remaining_duration;
    homekit_characteristic_t* set_duration;
    homekit_characteristic_t* in_use;
} valve_t;

valve_t valves[4];

void start_stop_task(bool);
void update_running(bool, valve_t*);
void update_valve_state(bool, valve_t*);
void on_update_active(homekit_characteristic_t *ch, homekit_value_t value, void *context);
void set_and_notify(homekit_characteristic_t *ch, const homekit_value_t value) {
    ch->value = value;
    homekit_characteristic_notify(ch, value);
}

void task_fn() {
    while(true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        for (int i=0; i < valve_count; i++) {
            valve_t valve = valves[i];
            if(valve.active->value.int_value == 1) {
                int new_duration = valve.remaining_duration->value.int_value - 1;
                // set without notify like the hap spec wants it :) 
                if(new_duration>0) {
                    valve.remaining_duration->value = HOMEKIT_UINT32(new_duration);
                }
                if(new_duration==0) {
                    set_and_notify(valve.remaining_duration, HOMEKIT_UINT32(new_duration));
                    set_and_notify(valve.active, HOMEKIT_UINT8(0));
                }
            }
        }
    }
}

void start_stop_task(bool start) {
    /* `task_running` ist just a helper because I can not access that 
     * information from `gate_task` with `vTaskGetInfo()`:
     * `undefined reference to vTaskGetInfo` from compiler 😕
     */
    if(start == true && task_running == false) {
        task_running = true;
        gpio_write(pump_gpio, 1);
        vTaskResume(gate_task);
    } 
    if(start == false) {
        bool idle = true;
        for (int i=0; i < valve_count; i++) {
            if(valves[i].active->value.int_value == 1 && valves[i].in_use->value.int_value == 1) {
                idle = false;
            }
        }
        if(idle == true) {
            task_running = false;
            gpio_write(pump_gpio, 1);
            vTaskSuspend(gate_task);
        }
    }
}

void update_running(bool target, valve_t *thisValve) {
    if(target == true) {
        update_valve_state(true, thisValve);
        set_and_notify(thisValve->remaining_duration, thisValve->set_duration->value);
        start_stop_task(true);
    } else {
        update_valve_state(false, thisValve);
        start_stop_task(false);
    }
}

void on_update_active(homekit_characteristic_t *ch, homekit_value_t value, void *context) {
    valve_t *thisValve = (valve_t *) context;
    if(value.int_value == 1) {
        update_running(true, thisValve);
    } else {
        update_running(false, thisValve);
    }
}

/* relay controllers */
void update_valve_state(bool state, valve_t *thisValve) {
    if(state) {
        printf("--> open gate number: %d \n", thisValve->gpio);
        gpio_write(thisValve->gpio, 1);
        set_and_notify(thisValve->in_use, HOMEKIT_UINT8(1));
    }
    else {
        printf("--> close gate number: %d \n", thisValve->gpio);
        gpio_write(thisValve->gpio, 0);
        set_and_notify(thisValve->in_use, HOMEKIT_UINT8(0));
    }
}

void valve_identify(homekit_value_t value) {
    printf("Valve identify\n");
}

void init_accessory() {
    homekit_service_t* services[MAX_SERVICES + 2];
    homekit_service_t** s = services;

    *(s++) = NEW_HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
        NEW_HOMEKIT_CHARACTERISTIC(NAME, "IrrigationSystem"),
        NEW_HOMEKIT_CHARACTERISTIC(MANUFACTURER, "buschco"),
        NEW_HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "123"),
        NEW_HOMEKIT_CHARACTERISTIC(MODEL, "Valve System 1"),
        NEW_HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1"),
        NEW_HOMEKIT_CHARACTERISTIC(IDENTIFY, valve_identify),
        NULL
    });

    *(s++) = NEW_HOMEKIT_SERVICE(SERVICE_LABEL, .characteristics=(homekit_characteristic_t*[]) {
        NEW_HOMEKIT_CHARACTERISTIC(SERVICE_LABEL_NAMESPACE, 1),
        NULL
    });

    gpio_enable(pump_gpio, GPIO_OUTPUT);
    gpio_write(pump_gpio, 0);

    for (int i=0; i < valve_count; i++) {
        valves[i].number = i;
        valves[i].gpio = valve_gpios[i];
        gpio_enable(valve_gpios[i], GPIO_OUTPUT);
        gpio_write(valve_gpios[i], 0);
        homekit_service_t *nextService = NEW_HOMEKIT_SERVICE(VALVE, .characteristics=(homekit_characteristic_t*[]) {
            NEW_HOMEKIT_CHARACTERISTIC(SERVICE_LABEL_INDEX, i+1),
            NEW_HOMEKIT_CHARACTERISTIC(VALVE_TYPE, 1),
            NEW_HOMEKIT_CHARACTERISTIC(
                ACTIVE,
                0,
                .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(
                    on_update_active, .context=(void*)&valves[i]
                ),
            ),
            NEW_HOMEKIT_CHARACTERISTIC(REMAINING_DURATION, 0),
            NEW_HOMEKIT_CHARACTERISTIC(SET_DURATION, 10),
            NEW_HOMEKIT_CHARACTERISTIC(IN_USE, 0),
            NULL
        });

        valves[i].active =  homekit_service_characteristic_by_type(nextService, "B0");
        valves[i].in_use = homekit_service_characteristic_by_type(nextService, "D2");
        valves[i].remaining_duration = homekit_service_characteristic_by_type(nextService, "D4");
        valves[i].set_duration = homekit_service_characteristic_by_type(nextService, "D3");
        *(s++) = nextService;
    }

    *(s++) = NULL;

    accessories[0] = NEW_HOMEKIT_ACCESSORY(.category=homekit_accessory_category_sprinkler, .services=services);
    accessories[1] = NULL;
}

homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111",
    .setupId="9CB8"
};

void user_init(void) {
    
    uart_set_baud(0, 115200);
    init_accessory();
    xTaskCreate(task_fn, "gate_task", 256, NULL, tskIDLE_PRIORITY, &gate_task);
    vTaskSuspend(gate_task);
    task_running = false;
    wifi_init();
    homekit_server_init(&config);
    printf("init complete\n");
}
