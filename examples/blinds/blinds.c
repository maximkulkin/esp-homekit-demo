/** @file

Motorized blinds.  Crude approach simply constant time to open or close a blind.  Since every one is different, there are different consants for left and right, and open and close.  This captures only some of the variation present.  This approach will never work 100%, there will always be some creepage or inconsistancy because there is no feedback from the blinds!

- Marc

*/

#include <stdio.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <esp/uart.h>
#include <esp8266.h>
#include <FreeRTOS.h>
#include <task.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include "wifi.h"

#define POSITION_STATIONARY 0
#define POSITION_JAMMED 1
#define POSITION_OSCILLATING 2


int right_timer = 0, left_timer = 0;	// blind rotation timers


homekit_value_t current_position_L_get();
homekit_value_t target_position_L_get();
homekit_value_t position_state_L_get();
homekit_value_t current_position_R_get();
homekit_value_t target_position_R_get();
homekit_value_t position_state_R_get();


void current_position_L_set(homekit_value_t value);
void target_position_L_set(homekit_value_t value);
void position_state_L_set(homekit_value_t value);
void current_position_R_set(homekit_value_t value);
void target_position_R_set(homekit_value_t value);
void position_state_R_set(homekit_value_t value);

void on_update_left(homekit_characteristic_t *ch, homekit_value_t value, void *context);
void on_update_right(homekit_characteristic_t *ch, homekit_value_t value, void *context);


homekit_characteristic_t current_position_left = HOMEKIT_CHARACTERISTIC_(
	CURRENT_POSITION, 0,
	.getter=current_position_L_get,
	.setter=current_position_L_set
);
homekit_characteristic_t target_position_left = HOMEKIT_CHARACTERISTIC_(
	TARGET_POSITION, 0,
	.getter=target_position_L_get,
	.setter=target_position_L_set,
	.callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_update_left)
);
homekit_characteristic_t position_state_left = HOMEKIT_CHARACTERISTIC_(
        POSITION_STATE, POSITION_STATIONARY,
        .getter=position_state_L_get,
        .setter=position_state_L_set
);
homekit_characteristic_t current_position_right = HOMEKIT_CHARACTERISTIC_(
	CURRENT_POSITION, 0,
	.getter=current_position_R_get,
	.setter=current_position_R_set
);
homekit_characteristic_t target_position_right = HOMEKIT_CHARACTERISTIC_(
	TARGET_POSITION, 0,
	.getter=target_position_R_get,
	.setter=target_position_R_set,
	.callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_update_right)
);
homekit_characteristic_t position_state_right = HOMEKIT_CHARACTERISTIC_(
        POSITION_STATE, POSITION_STATIONARY,
        .getter=position_state_R_get,
        .setter=position_state_R_set
);


static void wifi_init() {
    struct sdk_station_config wifi_config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASSWORD,
    };

    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&wifi_config);
    sdk_wifi_station_connect();
}


//pins
const int led_gpio = 2;
const int left_blind_close = 13;
const int left_blind_open = 12;
const int right_blind_close = 4;
const int right_blind_open = 14;
//const int remote_valid = 16;
const int remote_left_close = 15;
const int remote_left_open = 5;
const int remote_right_close = 16;
const int remote_right_open = 10;

const int poll_time = 50 / portTICK_PERIOD_MS;
const int blind_one_pct_time = (6400 / portTICK_PERIOD_MS) / 100; // total time divided by 100 - used for manual control 
const int left_blind_open_time = 4300 / portTICK_PERIOD_MS;
const int left_blind_close_time = 5900 / portTICK_PERIOD_MS;	// bias due to heavier motor load 
const int right_blind_open_time = 6000 / portTICK_PERIOD_MS;
const int right_blind_close_time = 7000 / portTICK_PERIOD_MS;	// bias due to heavier motor load closing

#define TIMER_TO_PCT_L_OPEN(x) ((x) * 100 / left_blind_open_time)
#define TIMER_TO_PCT_L_CLOSE(x) ((x) * 100 / left_blind_close_time)
#define TIMER_TO_PCT_R_OPEN(x) ((x) * 100 / right_blind_open_time)
#define TIMER_TO_PCT_R_CLOSE(x) ((x) * 100 / right_blind_close_time)

bool led_on = false;


void led_write(bool on) {
    gpio_write(led_gpio, on ? 0 : 1);
}

void led_init() {
    gpio_enable(led_gpio, GPIO_OUTPUT);
    led_write(led_on);
}

void main_task(void *_args) 
{
	gpio_enable(left_blind_close, GPIO_OUTPUT);
	gpio_enable(left_blind_open, GPIO_OUTPUT);
	gpio_enable(right_blind_close, GPIO_OUTPUT);
	gpio_enable(right_blind_open, GPIO_OUTPUT);
	
//	gpio_enable(remote_valid, GPIO_INPUT);
	gpio_enable(remote_left_close, GPIO_INPUT);
	gpio_enable(remote_left_open, GPIO_INPUT);
	gpio_enable(remote_right_close, GPIO_INPUT);
	gpio_enable(remote_right_open, GPIO_INPUT);
	
	
	while(1) 
	{
		led_write(false);
		
		if( current_position_right.value.int_value < target_position_right.value.int_value )
		{
			gpio_write(right_blind_open, true);
			gpio_write(right_blind_close, false);
			if( right_timer > 0 )
			{
				right_timer -= poll_time;
				if( right_timer <= 0 )
					right_timer = 0;
				
				if( target_position_right.value.int_value != current_position_right.value.int_value + TIMER_TO_PCT_R_OPEN(right_timer) )
				{
					current_position_right.value.int_value = target_position_right.value.int_value - TIMER_TO_PCT_R_OPEN(right_timer);
					homekit_characteristic_notify(&current_position_right, current_position_right.value);
					led_write(true);
					printf("open R current: %d target: %d timer %d\n", current_position_right.value.int_value, target_position_right.value.int_value, right_timer);
				}			
			}
			else
			{
				right_timer = poll_time;
			}
		}
		else if( current_position_right.value.int_value > target_position_right.value.int_value )
		{
			gpio_write(right_blind_open, false);
			gpio_write(right_blind_close, true);
			if( right_timer > 0 )
			{
				right_timer -= poll_time;
				if( right_timer <= 0 )
					right_timer = 0;
				
				if( target_position_right.value.int_value != current_position_right.value.int_value - TIMER_TO_PCT_R_CLOSE(right_timer) )
				{
					current_position_right.value.int_value = target_position_right.value.int_value + TIMER_TO_PCT_R_CLOSE(right_timer);
					homekit_characteristic_notify(&current_position_right, current_position_right.value);
					led_write(true);
					printf("close R current: %d target: %d timer %d\n", current_position_right.value.int_value, target_position_right.value.int_value, right_timer);
				}			
			}
			else
			{
				right_timer = poll_time;
			}
		}
		else
		{
			gpio_write(right_blind_open, false);
			gpio_write(right_blind_close, false);
		}

		if( current_position_left.value.int_value < target_position_left.value.int_value )
		{
			gpio_write(left_blind_open, true);
			gpio_write(left_blind_close, false);
			if( left_timer > 0 )
			{
				left_timer -= poll_time;
				if( left_timer <= 0 )
					left_timer = 0;
				
				if( target_position_left.value.int_value != current_position_left.value.int_value + TIMER_TO_PCT_L_OPEN(left_timer) )
				{
					current_position_left.value.int_value = target_position_left.value.int_value - TIMER_TO_PCT_L_OPEN(left_timer);
					homekit_characteristic_notify(&current_position_left, current_position_left.value);
					led_write(true);
					printf("open L current: %d target: %d timer %d\n", current_position_left.value.int_value, target_position_left.value.int_value, left_timer);
				}			
			}
			else
			{
				left_timer = poll_time;
			}
		}
		else if( current_position_left.value.int_value > target_position_left.value.int_value )
		{
			gpio_write(left_blind_open, false);
			gpio_write(left_blind_close, true);
			if( left_timer > 0 )
			{
				left_timer -= poll_time;
				if( left_timer <= 0 )
					left_timer = 0;
				
				if( target_position_left.value.int_value != current_position_left.value.int_value - TIMER_TO_PCT_L_CLOSE(left_timer) )
				{
					current_position_left.value.int_value = target_position_left.value.int_value + TIMER_TO_PCT_L_CLOSE(left_timer);
					homekit_characteristic_notify(&current_position_left, current_position_left.value);
					led_write(true);
					printf("close L current: %d target: %d timer %d\n", current_position_left.value.int_value, target_position_left.value.int_value, left_timer);
				}			
			}
			else
			{
				left_timer = poll_time;
			}
		}
		else
		{
			gpio_write(left_blind_open, false);
			gpio_write(left_blind_close, false);
		}


		//if(gpio_read(remote_valid))	// valid input from remote - not enough inputs!
		//{
			if( gpio_read(remote_left_close) )
			{
				if( target_position_left.value.int_value > target_position_left.min_value[0] )
				{
					if(target_position_left.value.int_value == current_position_left.value.int_value)
					{
						target_position_left.value.int_value = current_position_left.value.int_value - 1;
						homekit_characteristic_notify(&target_position_left, target_position_left.value);
						left_timer += blind_one_pct_time;
					}
				}	
				else	// allow remote to adjust close past limit
				{
					gpio_write(left_blind_open, false);
					gpio_write(left_blind_close, true);
				}
			}
			else if( gpio_read(remote_left_open) )
			{
				if( target_position_left.value.int_value < target_position_left.max_value[0] )
				{
					if(target_position_left.value.int_value == current_position_left.value.int_value)
					{
						target_position_left.value.int_value = current_position_left.value.int_value + 1;
						homekit_characteristic_notify(&target_position_left, target_position_left.value);
						left_timer += blind_one_pct_time;
					}
				}
				else	// allow remote to adjust open past limit
				{
					gpio_write(left_blind_open, true);
					gpio_write(left_blind_close, false);
				}
			}
			if( gpio_read(remote_right_close) )
			{					
				if( target_position_right.value.int_value > target_position_right.min_value[0] )
				{
					if(target_position_right.value.int_value == current_position_right.value.int_value )
					{
						target_position_right.value.int_value = current_position_right.value.int_value - 1;
						homekit_characteristic_notify(&target_position_right, target_position_right.value);
						right_timer += blind_one_pct_time;
					}
				}
				else	// allow remote to adjust close past limit
				{
					gpio_write(right_blind_open, false);
					gpio_write(right_blind_close, true);
				}
			}
			else if( gpio_read(remote_right_open) )
			{
				if( target_position_right.value.int_value < target_position_right.max_value[0] )
				{
					if(target_position_right.value.int_value == current_position_right.value.int_value)
					{
						target_position_right.value.int_value = current_position_right.value.int_value + 1;
						homekit_characteristic_notify(&target_position_right, target_position_right.value);
						right_timer += blind_one_pct_time;
					}
				}
				else	// allow remote to adjust open past limit
				{
					gpio_write(right_blind_open, true);
					gpio_write(right_blind_close, false);
				}
			}
		//}
		

		vTaskDelay(poll_time);
	}

}


void on_update_right(homekit_characteristic_t *ch, homekit_value_t value, void *context)
{
	int percent = current_position_right.value.int_value - target_position_right.value.int_value;
	
	if( percent < 0 ) 
		right_timer = right_blind_open_time * (-percent) / 100;
	else
		right_timer = right_blind_close_time * percent / 100;
	
	printf("R:current: %d target: %d timer %d\n", current_position_right.value.int_value, target_position_right.value.int_value, right_timer);
}

void on_update_left(homekit_characteristic_t *ch, homekit_value_t value, void *context)
{
	int percent = current_position_left.value.int_value - target_position_left.value.int_value;
	
	if( percent < 0 ) 
		left_timer = left_blind_open_time * (-percent) / 100;
	else
		left_timer = left_blind_close_time * percent / 100;
	
	printf("L:current: %d target: %d timer %d\n", current_position_left.value.int_value, target_position_left.value.int_value, left_timer);
}


void led_identify_task(void *_args) {
    for (int i=0; i<3; i++) {
        for (int j=0; j<2; j++) {
            led_write(true);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            led_write(false);
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

        vTaskDelay(250 / portTICK_PERIOD_MS);
    }

    led_write(led_on);

    vTaskDelete(NULL);
}

void led_identify(homekit_value_t _value) {
    printf("LED identify\n");
    xTaskCreate(led_identify_task, "LED identify", 128, NULL, 2, NULL);
}

homekit_value_t led_on_get() {
    return HOMEKIT_BOOL(led_on);
}

void led_on_set(homekit_value_t value) {
    if (value.format != homekit_format_bool) {
        printf("Invalid value format: %d\n", value.format);
        return;
    }

    led_on = value.bool_value;
    led_write(led_on);
}

homekit_value_t current_position_L_get() {
	return current_position_left.value;
}
homekit_value_t target_position_L_get() {
	return target_position_left.value;
}
homekit_value_t position_state_L_get() {
	return position_state_left.value;
}
homekit_value_t current_position_R_get() {
	return current_position_right.value;
}
homekit_value_t target_position_R_get() {
	return target_position_right.value;
}
homekit_value_t position_state_R_get() {
	return position_state_right.value;
}


void current_position_L_set(homekit_value_t value)
{
	current_position_left.value = value;
}
void target_position_L_set(homekit_value_t value)
{
	target_position_left.value = value;
}
void position_state_L_set(homekit_value_t value)
{
	position_state_left.value = value;
}
void current_position_R_set(homekit_value_t value)
{
	current_position_right.value = value;
}
void target_position_R_set(homekit_value_t value)
{
	target_position_right.value = value;
}
void position_state_R_set(homekit_value_t value)
{
	position_state_right.value = value;
}


homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_window_covering, .services=(homekit_service_t*[]){
        HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Blinds"),
            HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Bowery Engineering"),
            HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "0000001"),
            HOMEKIT_CHARACTERISTIC(MODEL, "EZ-Blinds"),
            HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1"),
            HOMEKIT_CHARACTERISTIC(IDENTIFY, led_identify),
            NULL
        }),
        HOMEKIT_SERVICE(WINDOW_COVERING, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Left Blind"),
            &current_position_left,
            &target_position_left,
            &position_state_left,
            NULL
        }),
        HOMEKIT_SERVICE(WINDOW_COVERING, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Right Blind"),
            &current_position_right,
            &target_position_right,
            &position_state_right,
            NULL
        }),
        NULL
    }),
    NULL
};



homekit_server_config_t config = {
    .accessories = accessories,
    .password = "111-11-111"
};

void user_init(void) {
    uart_set_baud(0, 115200);

    wifi_init();
    led_init();
    homekit_server_init(&config);
    xTaskCreate(main_task, "Main", 512, NULL, 2, NULL);
}
