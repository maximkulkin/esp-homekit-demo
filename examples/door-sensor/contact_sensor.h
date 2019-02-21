#pragma once

typedef enum {
    CONTACT_CLOSED,
    CONTACT_OPEN
} contact_sensor_state_t;

typedef void (*contact_sensor_callback_fn)(uint8_t gpio_num, contact_sensor_state_t event);

int contact_sensor_create(uint8_t gpio_num, contact_sensor_callback_fn callback);
void contact_sensor_destroy(uint8_t gpio_num);
contact_sensor_state_t contact_sensor_state_get(uint8_t gpio_num);
