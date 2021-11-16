/* Vovan's oil heater. PWM duty
 * File: vvduty.c
 * Started: Fri Oct 29 13:32:07 MSK 2021
 * Author: Max Amzarakov (maxam18 _at_ gmail _._ com)
 * Copyright (c) 2021 ..."
 */

#include <stdio.h>

#include "gorynych.h"

#include "esp_system.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include <vvduty.h>
#include <me_debug.h>

#define PWM_PIN_FAN          16
#define PWM_PIN_PUMP         17

int8_t  vv_duty[2] = { 0, 0 };

int8_t  vv_running = 0;

esp_err_t vv_duty_restore()
{
    nvs_handle  store;
    esp_err_t   err;

    err = nvs_open("vvduty", NVS_READONLY, &store);
    if (err != ESP_OK)
    {
        me_debug("DUTY", "Warning (%s) opening NVS handle. Using defaults.", esp_err_to_name(err));
        return err;
    }

    err = nvs_get_i8(store, "fan", &vv_duty[VV_IDX_FAN]);
    err += nvs_get_i8(store, "pump", &vv_duty[VV_IDX_PUMP]);
#ifdef CONFIG_ME_DEBUG
    if( err != ESP_OK )
        me_debug("DUTY", "pump duty read error (%s).", esp_err_to_name(err));
    else
        me_debug("DUTY", "successfuly read from storage");
#endif

    nvs_close(store);
    return err;
}

esp_err_t vv_duty_save()
{
    nvs_handle  store;
    esp_err_t   err;

    err = nvs_open("vvduty", NVS_READWRITE, &store);
    if (err != ESP_OK)
    {
        me_debug("DUTY", "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_i8(store, "fan", vv_duty[VV_IDX_FAN]);
    err += nvs_set_i8(store, "pump", vv_duty[VV_IDX_PUMP]);
#ifdef CONFIG_ME_DEBUG
    if( err != ESP_OK )
        me_debug("DUTY", "storage write error (%s).", esp_err_to_name(err));
    else
        me_debug("DUTY", "successfuly written to storage");
#endif
    nvs_close(store);

    return err;
}

void vv_duty_start()
{
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, vv_duty[0]);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, vv_duty[1]);
    vv_running = 1;
}

void vv_duty_stop()
{
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 0);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, 0);
    vv_running = 0;
}

void vv_change_duty(enum vv_duty_index idx, int8_t step)
{
    static mcpwm_generator_t gen[] = { MCPWM_OPR_A, MCPWM_OPR_B };
    
    int8_t val   = vv_duty[idx];

    val += step;
    if( val > 99 )
        val = 99;
    else if( val < 0 )
        val = 0;
    
    vv_duty[idx] = val;

    if( vv_duty_run() )
        mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, gen[idx], val);
}

void vv_duty_init(void)
{
    mcpwm_config_t      pwm_conf = {
        .frequency    = 10,
        .cmpr_a       = 0,
        .cmpr_b       = 0,
        .counter_mode = MCPWM_UP_COUNTER,
        .duty_mode    = MCPWM_DUTY_MODE_0
    };

    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, PWM_PIN_FAN);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0B, PWM_PIN_PUMP);
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_conf);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 0);
    mcpwm_set_duty(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_B, 0);
    //mcpwm_start(MCPWM_UNIT_0, MCPWM_TIMER_0);
}