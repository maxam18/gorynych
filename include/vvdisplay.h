/* Vovan's oil heater. Display
 * File: vvdisplay.h
 * Started: Thu Oct 28 19:50:45 MSK 2021
 * Author: Max Amzarakov (maxam18 _at_ gmail _._ com)
 * Copyright (c) 2021 ..."
 */

#ifndef _VVDISPLAY_H
#define _VVDISPLAY_H

typedef enum { 
    VV_SHOW_ON,
    VV_SHOW_FAN, 
    VV_SHOW_PUMP, 
    VV_SHOW_TEMP, 
    VV_SHOW_RH
} vv_disp_enum_t;

void vv_disp_set(vv_disp_enum_t what);
void vv_disp_freeze(uint8_t sw);
void vv_disp_update(vv_disp_enum_t what, int8_t val);
void vv_disp_update_run(uint8_t is_running);
void vv_disp_set(vv_disp_enum_t what);
void vv_disp_init();

#endif

