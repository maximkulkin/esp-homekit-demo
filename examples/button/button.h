#pragma once

typedef enum {
    button_event_single_press,
    button_event_double_press,
    button_event_long_press,
} button_event_t;

typedef void (*button_callback_fn)(uint8_t gpio_num, button_event_t event);

int button_create(uint8_t gpio_num, button_callback_fn callback);
void button_destroy(uint8_t gpio_num);
