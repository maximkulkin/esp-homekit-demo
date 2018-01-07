/******************************************************************************
 * Copyright 2015 Vowstar Co.,Ltd.
 *
 * FileName: mjpwm.c
 *
 * Description: MJPWM Driver
 *
 * Modification history:
 *     2015/09/10, v1.0 create this file.
 *     ??????????, found in noduino sources
 *     2017/12/24, adapted for esp-open-rtos
*******************************************************************************/
#include "mjpwm.h"
#include <espressif/esp_misc.h>  //defines sdk_os_delay_us
#include <task.h>
#include <esp/gpio.h>

#define GPIO_MAX_INDEX 16

#ifndef LOW
#define LOW                             (0)
#endif              /* ifndef LOW */

#ifndef HIGH
#define HIGH                            (1)
#endif              /* ifndef HIGH */

//#define MJPWM_DIRECT_GPIO(pin)        PIN_FUNC_SELECT(pin_name[pin], pin_func[pin])
#define MJPWM_DIRECT_GPIO(pin)          gpio_enable(pin,GPIO_OUTPUT)
////#define MJPWM_DIRECT_READ(pin)      (0x01 & GPIO_INPUT_GET(GPIO_ID_PIN(pin)))
////#define MJPWM_DIRECT_MODE_INPUT(pin)    (GPIO_DIS_OUTPUT(GPIO_ID_PIN(pin)))
#define MJPWM_DIRECT_MODE_OUTPUT(pin)
//#define MJPWM_DIRECT_WRITE_LOW(pin)   (GPIO_OUTPUT_SET(GPIO_ID_PIN(pin), LOW))
#define MJPWM_DIRECT_WRITE_LOW(pin)     gpio_write(pin,0)
//#define MJPWM_DIRECT_WRITE_HIGH(pin)  (GPIO_OUTPUT_SET(GPIO_ID_PIN(pin), HIGH))
#define MJPWM_DIRECT_WRITE_HIGH(pin)    gpio_write(pin,1)


static int nc = 2;

static uint8_t pin_di = 13;
static uint8_t pin_dcki = 15;

static mjpwm_cmd_t mjpwm_commands[GPIO_MAX_INDEX + 1];

IRAM void mjpwm_di_pulse(uint16_t times)
{
    uint16_t i;
    for (i = 0; i < times; i++) {
        MJPWM_DIRECT_WRITE_HIGH(pin_di);
        asm("nop;");    // delay 50ns
        MJPWM_DIRECT_WRITE_LOW(pin_di);
        asm("nop;nop;nop;nop;nop;");
        // delay 230ns
    }
}

void mjpwm_dcki_pulse(uint16_t times)
{
    uint16_t i;
    for (i = 0; i < times; i++) {
        MJPWM_DIRECT_WRITE_HIGH(pin_dcki);
        asm("nop;");        // delay 50ns
        MJPWM_DIRECT_WRITE_LOW(pin_dcki);
        asm("nop;");        // delay 50ns
    }
}

void mjpwm_send_command(mjpwm_cmd_t command)
{
    uint8_t i, n;
    uint8_t command_data;
    mjpwm_commands[pin_dcki] = command;

    taskENTER_CRITICAL(); //ets_intr_lock();
    // TStop > 12us.
    sdk_os_delay_us(12);
    // Send 12 DI pulse, after 6 pulse's falling edge store duty data, and 12
    // pulse's rising edge convert to command mode.
    mjpwm_di_pulse(12);
    // Delay >12us, begin send CMD data
    sdk_os_delay_us(12);
    asm("nop;nop;");
    // Send CMD data

    for (n = 0; n < nc; n++) {

        command_data = *(uint8_t *) (&command);

        for (i = 0; i < 4; i++) {
            // DCK = 0;
            MJPWM_DIRECT_WRITE_LOW(pin_dcki);
            if (command_data & 0x80) {
                // DI = 1;
                MJPWM_DIRECT_WRITE_HIGH(pin_di);
            } else {
                // DI = 0;
                MJPWM_DIRECT_WRITE_LOW(pin_di);
            }
            // DCK = 1;
            MJPWM_DIRECT_WRITE_HIGH(pin_dcki);
            command_data = command_data << 1;
            if (command_data & 0x80) {
                // DI = 1;
                MJPWM_DIRECT_WRITE_HIGH(pin_di);
            } else {
                // DI = 0;
                MJPWM_DIRECT_WRITE_LOW(pin_di);
            }
            // DCK = 0;
            MJPWM_DIRECT_WRITE_LOW(pin_dcki);
            // DI = 0;
            MJPWM_DIRECT_WRITE_LOW(pin_di);
            command_data = command_data << 1;
        }

        asm("nop;nop;nop;");
    }

    // TStart > 12us. Delay 12 us.
    sdk_os_delay_us(12);
    // Send 16 DI pulse，at 14 pulse's falling edge store CMD data, and
    // at 16 pulse's falling edge convert to duty mode.
    mjpwm_di_pulse(16);
    // TStop > 12us.
    sdk_os_delay_us(12);
    asm("nop;nop;");
    taskEXIT_CRITICAL(); //ets_intr_unlock();
}

IRAM void mjpwm_send_duty(uint16_t duty_r, uint16_t duty_g,
        uint16_t duty_b, uint16_t duty_w)
{
    uint8_t i = 0, n;
    uint8_t channel = 0;
    uint8_t bit_length = 8;
    uint16_t duty_current = 0;

    // Definition for RGBW channels
    uint16_t duty[4] = { duty_r, duty_g, duty_b, duty_w };

    switch (mjpwm_commands[pin_dcki].bit_width) {
    case MJPWM_CMD_BIT_WIDTH_16:
        bit_length = 16;
        break;
    case MJPWM_CMD_BIT_WIDTH_14:
        bit_length = 14;
        break;
    case MJPWM_CMD_BIT_WIDTH_12:
        bit_length = 12;
        break;
    case MJPWM_CMD_BIT_WIDTH_8:
        bit_length = 8;
        break;
    default:
        bit_length = 8;
        break;
    }

    taskENTER_CRITICAL(); //ets_intr_lock();
    // TStop > 12us.
    sdk_os_delay_us(12);
    asm("nop;nop;");

    for (n = 0; n < nc; n++)
    {
        for (channel = 0; channel < 4; channel++)   //RGBW 4CH
        {
            // RGBW Channel
            duty_current = duty[channel];
            // Send 8bit/12bit/14bit/16bit Data
            for (i = 0; i < bit_length / 2; i++) {

                // DCK = 0;
                MJPWM_DIRECT_WRITE_LOW(pin_dcki);
                if (duty_current & (0x01 << (bit_length - 1))) {
                    // DI = 1;
                    MJPWM_DIRECT_WRITE_HIGH(pin_di);
                } else {
                    // DI = 0;
                    MJPWM_DIRECT_WRITE_LOW(pin_di);
                }

                // DCK = 1;
                MJPWM_DIRECT_WRITE_HIGH(pin_dcki);
                duty_current = duty_current << 1;
                if (duty_current & (0x01 << (bit_length - 1))) {
                    // DI = 1;
                    MJPWM_DIRECT_WRITE_HIGH(pin_di);
                } else {
                    // DI = 0;
                    MJPWM_DIRECT_WRITE_LOW(pin_di);
                }

                //DCK = 0;
                MJPWM_DIRECT_WRITE_LOW(pin_dcki);
                //DI = 0;
                MJPWM_DIRECT_WRITE_LOW(pin_di);

                duty_current = duty_current << 1;
            }
        }
    }

    // TStart > 12us. Ready for send DI pulse.
    sdk_os_delay_us(12);
    // Send 8 DI pulse. After 8 pulse falling edge, store old data.
    mjpwm_di_pulse(8);
    // TStop > 12us.
    sdk_os_delay_us(12);
    asm("nop;nop;");
    taskEXIT_CRITICAL(); //ets_intr_unlock();
}

void mjpwm_init(uint8_t di, uint8_t dcki, uint8_t n_chips, mjpwm_cmd_t cmd)
{
    pin_di = di;
    pin_dcki = dcki;

    MJPWM_DIRECT_GPIO(pin_di);
    MJPWM_DIRECT_GPIO(pin_dcki);
    MJPWM_DIRECT_MODE_OUTPUT(pin_di);
    MJPWM_DIRECT_MODE_OUTPUT(pin_dcki);
    MJPWM_DIRECT_WRITE_LOW(pin_di);
    MJPWM_DIRECT_WRITE_LOW(pin_dcki);

    nc = n_chips;

    // Clear all duty register
    mjpwm_dcki_pulse(32 * nc);

    mjpwm_send_command(cmd);
}
