/* Vovan's oil heater. Main prog
 * File: gorynych.h
 * Started: Thu Oct 28 19:50:23 MSK 2021
 * Author: Max Amzarakov (maxam18 _at_ gmail _._ com)
 * Copyright (c) 2021 ..."
 */

#ifndef _VVOWEN_H
#define _VVOWEN_H

#include "sdkconfig.h"
#include <me_bme280.h>

#define vv_get_running()    is_running

void vv_set_running(uint8_t to);
void gorynych(void);
extern me_bme280_readings_t bme280_data;
extern uint8_t              is_running;
#endif