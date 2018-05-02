#pragma once



typedef void (*toggle_callback_fn)(uint8_t gpio_num);

/** 
    Starts monitoring the given GPIO pin for change of state. Events are recieved through the callback.

    @param gpio_num The GPIO pin that should be monitored
    @param callback The callback that is called when an "toggle" event occurs.
    @return A negative integer if this method fails.
*/
int toggle_create(uint8_t gpio_num, toggle_callback_fn callback);

/** 
    Removes the given GPIO pin from monitoring.

    @param gpio_num The GPIO pin that should be removed from monitoring
*/
void toggle_delete(uint8_t gpio_num);
