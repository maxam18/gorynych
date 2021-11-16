/* Vovan's oil heater. PWM duty
 * File: vvduty.h
 * Started: Fri Oct 29 13:31:58 MSK 2021
 * Author: Max Amzarakov (maxam18 _at_ gmail _._ com)
 * Copyright (c) 2021 ..."
 */

#ifndef _VVDUTY_H
#define _VVDUTY_H

#include <esp_system.h>
#include "driver/mcpwm.h"

enum vv_duty_index { VV_IDX_FAN, VV_IDX_PUMP };
extern int8_t vv_duty[];
extern int8_t vv_running;

#define vv_duty_run()       (vv_running == 1)

esp_err_t vv_duty_save();
esp_err_t vv_duty_restore();
void vv_change_duty(enum vv_duty_index idx, int8_t step);
void vv_duty_init(void);
void vv_duty_start(void);
void vv_duty_stop(void);

#endif

