#include <string.h>
#include <esplibs/libmain.h>
#include "toggle.h"


typedef struct _toggle {
    uint8_t gpio_num;
    toggle_callback_fn callback;

    uint16_t debounce_time;
    uint8_t state;
    uint32_t last_event_time;

    struct _toggle *next;
} toggle_t;


toggle_t *toggles = NULL;


static toggle_t *toggle_find_by_gpio(const uint8_t gpio_num) {
    toggle_t *toggle = toggles;
    while (toggle && toggle->gpio_num != gpio_num)
        toggle = toggle->next;

    return toggle;
}





void toggle_intr_callback(uint8_t gpio) {
    toggle_t *toggle = toggle_find_by_gpio(gpio);
    if (!toggle)
        return;

    uint32_t now = xTaskGetTickCountFromISR();
    if ((now - toggle->last_event_time)*portTICK_PERIOD_MS < toggle->debounce_time) {
        // debounce time, ignore events
        return;
    }
    toggle->last_event_time = now;
    if (gpio_read(toggle->gpio_num) != toggle->state) {
        // different state = toggled
        toggle->state = gpio_read(toggle->gpio_num);
        toggle->callback(toggle->gpio_num);
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

    // times in milliseconds
    toggle->debounce_time = 50;

    // initial state is as initilised
    toggle->state = gpio_read(gpio_num);

    uint32_t now = xTaskGetTickCountFromISR();
    toggle->last_event_time = now;

    toggle->next = toggles;
    toggles = toggle;

    gpio_enable(toggle->gpio_num, GPIO_INPUT);
    gpio_set_pullup(toggle->gpio_num, true, true);
    gpio_set_interrupt(toggle->gpio_num, GPIO_INTTYPE_EDGE_ANY, toggle_intr_callback);

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

    if (toggle) {
        gpio_set_interrupt(gpio_num, GPIO_INTTYPE_EDGE_ANY, NULL);
    }
}

