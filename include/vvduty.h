/* Vovan's oil heater. PWM duty
 * File: vvduty.h
 * Started: Fri Oct 29 13:31:58 MSK 2021
 * Author: Max Amzarakov (maxam18 _at_ gmail _._ com)
 * Copyright (c) 2021 ..."
 */

#ifndef _VVDUTY_H
#define _VVDUTY_H

#include <esp_system.h>

enum vv_duty_index { VV_IDX_FAN, VV_IDX_PUMP };
extern int8_t vv_duty[];

esp_err_t vv_save_duty();
esp_err_t vv_restore_duty();
void vv_change_duty(enum vv_duty_index idx, int8_t step);
void vv_reset_duty();
#define vv_start_duty vv_reset_duty
void vv_stop_duty();
void vv_duty_init(void);

#endif

