#include <string.h>
#include <esplibs/libmain.h>
#include "toggle.h"

#define LPF_SHIFT 3  // divide by 8
#define LPF_INTERVAL 10  // in milliseconds

#define maxvalue_unsigned(x) ((1<<(8*sizeof(x)))-1)

typedef struct _toggle {
    uint8_t gpio_num;
    toggle_callback_fn callback;

    uint8_t state;
    uint16_t value;
    uint32_t last_event_time;

    TaskHandle_t task_handle;
    struct _toggle *next;
} toggle_t;


toggle_t *toggles = NULL;


static toggle_t *toggle_find_by_gpio(const uint8_t gpio_num) {
    toggle_t *toggle = toggles;
    while (toggle && toggle->gpio_num != gpio_num)
        toggle = toggle->next;

    return toggle;
}

void toggleService(void *_args) {
    const TickType_t xPeriod = pdMS_TO_TICKS(LPF_INTERVAL);
    TickType_t xLastWakeTime = xTaskGetTickCount();
    
    uint8_t gpio = (uint8_t)((uint32_t)_args);
    toggle_t *toggle = toggle_find_by_gpio(gpio);
    if (!toggle)
        vTaskDelete(NULL);

    for (;;) {
        toggle->value += ((gpio_read(toggle->gpio_num) * maxvalue_unsigned(toggle->value)) - toggle->value) >> LPF_SHIFT ;
        uint8_t state = (toggle->value > (maxvalue_unsigned(toggle->value) / 2));

        if (state != toggle->state) {
            // different state = toggled
            toggle->state = state;
            toggle->callback(toggle->gpio_num);
        }

        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

int toggle_create(const uint8_t gpio_num, toggle_callback_fn callback) {
    toggle_t *toggle = toggle_find_by_gpio(gpio_num);
    if (toggle)
        return -1;

    toggle = malloc(sizeof(toggle_t));
    memset(toggle, 0, sizeof(*toggle));
    toggle->gpio_num = gpio_num;
    toggle->callback = callback;

    // initial state is as initilised
    toggle->state = gpio_read(gpio_num);

    uint32_t now = xTaskGetTickCountFromISR();
    toggle->last_event_time = now;

    toggle->next = toggles;
    toggles = toggle;

    gpio_set_pullup(toggle->gpio_num, true, true);

    char service_name[16];
    snprintf(service_name, sizeof service_name, "%s%d", "toggleService", gpio_num);
    BaseType_t task_created = xTaskCreate(toggleService, service_name, 255, (void *)((uint32_t)gpio_num), 2, &toggle->task_handle);
    if (task_created != pdPASS)
        return -1;

    return 0;
}

void toggle_delete(const uint8_t gpio_num) {
    if (!toggles)
        return;

    toggle_t *toggle = NULL;
    if (toggles->gpio_num == gpio_num) {
        toggle = toggles;
        toggles = toggles->next;
    } else {
        toggle_t *b = toggles;
        while (b->next) {
            if (b->next->gpio_num == gpio_num) {
                toggle = b->next;
                b->next = b->next->next;
                break;
            }
        }
    }

    if (toggle)
        vTaskDelete(toggle->task_handle);
}
