/*  (c) 2018 HomeAccessoryKid
 *  This example makes an RGBW smart lightbulb as offered on e.g. alibaba
 *  with the brand of ZemiSmart. It uses an ESP8266 with a 1MB flash on a 
 *  TYLE1R printed circuit board by TuyaSmart (also used in AiLight).
 *  There are terminals with markings for GND, 3V3, Tx, Rx and IO0
 *  There's a second GND terminal that can be used to set IO0 for flashing
 *  Popping of the plastic cap is sometimes hard, but never destructive
 *  Note that the LED color is COLD white.
 *  Changing them for WARM is possible but requires skill and nerves.
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

#include <math.h>  //requires LIBS ?= hal m to be added to Makefile
#include "mjpwm.h"


static void wifi_init() {
    struct sdk_station_config wifi_config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASSWORD,
    };

    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&wifi_config);
    sdk_wifi_station_connect();
}

//http://blog.saikoled.com/post/44677718712/how-to-convert-from-hsi-to-rgb-white
void hsi2rgbw(float h, float s, float i, int* rgbw) {
    int r, g, b, w;
    float cos_h, cos_1047_h;
    //h = fmod(h,360); // cycle h around to 0-360 degrees
    h = 3.14159*h/(float)180; // Convert to radians.
    s /=(float)100; i/=(float)100; //from percentage to ratio
    s = s>0?(s<1?s:1):0; // clamp s and i to interval [0,1]
    i = i>0?(i<1?i:1):0;
    i = i*sqrt(i); //shape intensity to have finer granularity near 0

    if(h < 2.09439) {
        cos_h = cos(h);
        cos_1047_h = cos(1.047196667-h);
        r = s*4095*i/3*(1+cos_h/cos_1047_h);
        g = s*4095*i/3*(1+(1-cos_h/cos_1047_h));
        b = 0;
        w = 4095*(1-s)*i;
    } else if(h < 4.188787) {
        h = h - 2.09439;
        cos_h = cos(h);
        cos_1047_h = cos(1.047196667-h);
        g = s*4095*i/3*(1+cos_h/cos_1047_h);
        b = s*4095*i/3*(1+(1-cos_h/cos_1047_h));
        r = 0;
        w = 4095*(1-s)*i;
    } else {
        h = h - 4.188787;
        cos_h = cos(h);
        cos_1047_h = cos(1.047196667-h);
        b = s*4095*i/3*(1+cos_h/cos_1047_h);
        r = s*4095*i/3*(1+(1-cos_h/cos_1047_h));
        g = 0;
        w = 4095*(1-s)*i;
    }

    rgbw[0]=r;
    rgbw[1]=g;
    rgbw[2]=b;
    rgbw[3]=w;
}

#define PIN_DI 				13
#define PIN_DCKI 			15

float hue,sat,bri;
bool on;

void lightSET(void) {
    int rgbw[4];
    if (on) {
        printf("h=%d,s=%d,b=%d => ",(int)hue,(int)sat,(int)bri);
        
        hsi2rgbw(hue,sat,bri,rgbw);
        printf("r=%d,g=%d,b=%d,w=%d\n",rgbw[0],rgbw[1],rgbw[2],rgbw[3]);
        
        mjpwm_send_duty(rgbw[0],rgbw[1],rgbw[2],rgbw[3]);
    } else {
        printf("off\n");
        mjpwm_send_duty(     0,      0,      0,      0 );
    }
}

void light_init() {
    mjpwm_cmd_t init_cmd = {
        .scatter = MJPWM_CMD_SCATTER_APDM,
        .frequency = MJPWM_CMD_FREQUENCY_DIVIDE_1,
        .bit_width = MJPWM_CMD_BIT_WIDTH_12,
        .reaction = MJPWM_CMD_REACTION_FAST,
        .one_shot = MJPWM_CMD_ONE_SHOT_DISABLE,
        .resv = 0,
    };
    mjpwm_init(PIN_DI, PIN_DCKI, 1, init_cmd);
    on=true; hue=0; sat=0; bri=100; //this should not be here, but part of the homekit init work
    lightSET();
}

homekit_value_t light_on_get() {
    return HOMEKIT_BOOL(on);
}
void light_on_set(homekit_value_t value) {
    if (value.format != homekit_format_bool) {
        printf("Invalid on-value format: %d\n", value.format);
        return;
    }
    on = value.bool_value;
    lightSET();
}

homekit_value_t light_bri_get() {
    return HOMEKIT_INT(bri);
}
void light_bri_set(homekit_value_t value) {
    if (value.format != homekit_format_int) {
        printf("Invalid bri-value format: %d\n", value.format);
        return;
    }
    bri = value.int_value;
    lightSET();
}

homekit_value_t light_hue_get() {
    return HOMEKIT_FLOAT(hue);
}
void light_hue_set(homekit_value_t value) {
    if (value.format != homekit_format_float) {
        printf("Invalid hue-value format: %d\n", value.format);
        return;
    }
    hue = value.float_value;
    lightSET();
}

homekit_value_t light_sat_get() {
    return HOMEKIT_FLOAT(sat);
}
void light_sat_set(homekit_value_t value) {
    if (value.format != homekit_format_float) {
        printf("Invalid sat-value format: %d\n", value.format);
        return;
    }
    sat = value.float_value;
    lightSET();
}


void light_identify_task(void *_args) {
    for (int i=0;i<5;i++) {
        mjpwm_send_duty(4095,    0,    0,    0);
        vTaskDelay(300 / portTICK_PERIOD_MS); //0.3 sec
        mjpwm_send_duty(   0, 4095,    0,    0);
        vTaskDelay(300 / portTICK_PERIOD_MS); //0.3 sec
        mjpwm_send_duty(   0,    0, 4095,    0);
        vTaskDelay(300 / portTICK_PERIOD_MS); //0.3 sec
    }
    lightSET();

    vTaskDelete(NULL);
}

void light_identify(homekit_value_t _value) {
    printf("Light Identify\n");
    xTaskCreate(light_identify_task, "Light identify", 256, NULL, 2, NULL);
}


homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(
        .id=1,
        .category=homekit_accessory_category_lightbulb,
        .services=(homekit_service_t*[]){
            HOMEKIT_SERVICE(ACCESSORY_INFORMATION,
                .characteristics=(homekit_characteristic_t*[]){
                    HOMEKIT_CHARACTERISTIC(NAME, "Light"),
                    HOMEKIT_CHARACTERISTIC(MANUFACTURER, "HacK"),
                    HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "1"),
                    HOMEKIT_CHARACTERISTIC(MODEL, "ZemiSmart"),
                    //HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "0.1"),
                    HOMEKIT_CHARACTERISTIC(IDENTIFY, light_identify),
                    NULL
                }),
            HOMEKIT_SERVICE(LIGHTBULB, .primary=true,
                .characteristics=(homekit_characteristic_t*[]){
                    HOMEKIT_CHARACTERISTIC(
                        ON, true,
                        .getter=light_on_get,
                        .setter=light_on_set
                    ),
                    HOMEKIT_CHARACTERISTIC(
                        BRIGHTNESS, 100,
                        .getter=light_bri_get,
                        .setter=light_bri_set
                    ),
                    HOMEKIT_CHARACTERISTIC(
                        HUE, 0,
                        .getter=light_hue_get,
                        .setter=light_hue_set
                    ),
                    HOMEKIT_CHARACTERISTIC(
                        SATURATION, 0,
                        .getter=light_sat_get,
                        .setter=light_sat_set
                    ),
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
    light_init();
    homekit_server_init(&config);
}
