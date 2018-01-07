/******************************************************************************
 * Copyright 2015 Vowstar Co.,Ltd.
 *
 * FileName: mjpwm.h
 *
 * Description: MJPWM Driver
 *
 * Modification history:
 *     2015/09/10, v1.0 create this file.
 *     ??????????, found in noduino sources
 *     2017/12/24, adapted for esp-open-rtos
*******************************************************************************/

#ifndef __MJPWM_H__
#define __MJPWM_H__

#include <FreeRTOS.h>  //added for esp-open-rtos

typedef enum mjpwm_cmd_one_shot_t {
    MJPWM_CMD_ONE_SHOT_DISABLE = 0X00,
    MJPWM_CMD_ONE_SHOT_ENFORCE = 0X01,
} mjpwm_cmd_one_shot_t;

typedef enum mjpwm_cmd_reaction_t {
    MJPWM_CMD_REACTION_FAST = 0X00,
    MJPWM_CMD_REACTION_SLOW = 0X01,
}  mjpwm_cmd_reaction_t;

typedef enum mjpwm_cmd_bit_width_t {
    MJPWM_CMD_BIT_WIDTH_16 = 0X00,
    MJPWM_CMD_BIT_WIDTH_14 = 0X01,
    MJPWM_CMD_BIT_WIDTH_12 = 0X02,
    MJPWM_CMD_BIT_WIDTH_8 = 0X03,
} mjpwm_cmd_bit_width_t;

typedef enum mjpwm_cmd_frequency_t {
    MJPWM_CMD_FREQUENCY_DIVIDE_1 = 0X00,
    MJPWM_CMD_FREQUENCY_DIVIDE_4 = 0X01,
    MJPWM_CMD_FREQUENCY_DIVIDE_16 = 0X02,
    MJPWM_CMD_FREQUENCY_DIVIDE_64 = 0X03,
} mjpwm_cmd_frequency_t;

typedef enum mjpwm_cmd_scatter_t {
    MJPWM_CMD_SCATTER_APDM = 0X00,
    MJPWM_CMD_SCATTER_PWM = 0X01,
} mjpwm_cmd_scatter_t;

typedef struct mjpwm_cmd_t {
    mjpwm_cmd_scatter_t scatter: 1;
    mjpwm_cmd_frequency_t frequency: 2;
    mjpwm_cmd_bit_width_t bit_width: 2;
    mjpwm_cmd_reaction_t reaction: 1;
    mjpwm_cmd_one_shot_t one_shot: 1;
    uint8_t resv: 1;
} __attribute__((aligned(1), packed)) mjpwm_cmd_t;

#define MJPWM_COMMAND_DEFAULT \
{ \
    .scatter = mjpwm_cmd_scatter_apdm, \
    .frequency = mjpwm_cmd_frequency_divide_1, \
    .bit_width = mjpwm_cmd_bit_width_8, \
    .reaction = mjpwm_cmd_reaction_fast, \
    .one_shot = mjpwm_cmd_one_shot_disable, \
    .resv = 0, \
}

void mjpwm_init(uint8_t pin_di, uint8_t pin_dcki, uint8_t n_chips, mjpwm_cmd_t command);
void mjpwm_di_pulse(uint16_t times);
void mjpwm_dcki_pulse(uint16_t times);
void mjpwm_send_command(mjpwm_cmd_t command);
void mjpwm_send_duty(uint16_t duty_r, uint16_t duty_g, uint16_t duty_b, uint16_t duty_w);

#endif /* __MJPWM_H__ */
