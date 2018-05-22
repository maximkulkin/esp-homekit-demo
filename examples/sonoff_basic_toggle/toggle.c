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

    struct _toggle *next;
} toggle_t;


toggle_t *toggles = NULL;
TaskHandle_t task_handle = NULL;

static toggle_t *toggle_find_by_gpio(const uint8_t gpio_num) {
    toggle_t *toggle = toggles;
    while (toggle && toggle->gpio_num != gpio_num)
        toggle = toggle->next;

    return toggle;
}

void toggleService(void *_args) {
    const TickType_t xPeriod = pdMS_TO_TICKS(LPF_INTERVAL);
    TickType_t xLastWakeTime = xTaskGetTickCount();

    for (;;) {
        toggle_t *toggle = toggles;
        uint8_t state = 0;
        
        while (toggle) {
            toggle->value += ((gpio_read(toggle->gpio_num) * maxvalue_unsigned(toggle->value)) - toggle->value) >> LPF_SHIFT ;
            state = (toggle->value > (maxvalue_unsigned(toggle->value) / 2));

            if (state != toggle->state) {
                toggle->state = state;
                toggle->callback(toggle->gpio_num);
            }
            
            toggle = toggle->next;
        }

        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

int toggle_create(const uint8_t gpio_num, toggle_callback_fn callback) {
    if (task_handle == NULL) {
        BaseType_t created = xTaskCreate(toggleService, "toggleService", 255, NULL, 2, &task_handle);
        if (created != pdPASS) {
            task_handle = NULL;   
            return -1;
        }
    }
    
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
    
    return 0;
}

void toggle_delete(const uint8_t gpio_num) {
    if (!toggles)
        return;

    if (toggles->gpio_num == gpio_num) {
        toggles = toggles->next;
    } else {
        toggle_t *b = toggles;
        while (b->next) {
            if (b->next->gpio_num == gpio_num) {
                b->next = b->next->next;
                break;
            }
        }
    }
}
