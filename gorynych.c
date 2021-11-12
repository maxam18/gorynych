/* Vovan's oil heater. Main prog
 * File: gorynych.c
 * Started: Thu Oct 28 19:50:12 MSK 2021
 * Author: Max Amzarakov (maxam18 _at_ gmail _._ com)
 * Copyright (c) 2021 ..."
 */

#include <stdio.h>
#include "gorynych.h"

#include "esp_system.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_intr_alloc.h"
#include "driver/gpio.h"
#include "driver/mcpwm.h"
#include "esp_netif.h"
#include "mdns.h"

#include <me_wifi_ap.h>
#include <vvduty.h>
#include <vvhttp.h>
#include <vvdisplay.h>

#define VV_BTN_NONE        0
#define VV_BTN_MAIN        34
#define VV_BTN_LEFT        33
#define VV_BTN_RIGHT       35

#define VV_SDA_PIN         21
#define VV_SCL_PIN         22

#define HOLD_DELAY         pdMS_TO_TICKS(1500)
#define UPDATE_DELAY       pdMS_TO_TICKS(15000)

static xQueueHandle         queue = NULL;
static me_bme280_conf_t     bme280_conf[1] = {{ 
                                      .port = I2C_NUM_0
                                    , .addr = ME_BME280_I2C_ADDR_PRIMARY
                                }};
me_bme280_readings_t        bme280_data;
uint8_t                     is_running = 0;
static const char          *TAG = "VV";

void vv_set_running(uint8_t to)
{
    if( is_running == to )
        return;

    is_running = to;
    if( is_running )
        vv_start_duty();
    else
        vv_stop_duty();

    vv_disp_update_run(is_running);
}

static void IRAM_ATTR btn_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(queue, &gpio_num, NULL);
}

static void vvowen_task(void *ign)
{
    vv_disp_enum_t          disp = VV_SHOW_TEMP;
    uint8_t                 is_edit = 0;
    uint32_t                btn   = VV_BTN_NONE;
    TickType_t              delay = UPDATE_DELAY;

    while( 1 )
    {
        if( xQueueReceive(queue, &btn, delay) )
        {
            vTaskDelay(pdMS_TO_TICKS(200));
            if( gpio_get_level(btn) == 0 )
            {
                btn = VV_BTN_NONE;
                continue;
            }

            delay = HOLD_DELAY;
            switch( btn ) {
                case VV_BTN_MAIN:
                break;
                case VV_BTN_LEFT:
                    if( is_edit ) {
                        vv_change_duty(disp - VV_SHOW_FAN, -1);
                        vv_disp_update(disp, vv_duty[disp-VV_SHOW_FAN]);
                        if( is_running )
                            vv_reset_duty();
                    } else if( disp == VV_SHOW_ON )
                        disp = VV_SHOW_RH;
                    else
                        disp--;
                break;
                case VV_BTN_RIGHT:
                    if( is_edit ) {
                        vv_change_duty(disp - VV_SHOW_FAN, 1);
                        vv_disp_update(disp, vv_duty[disp-VV_SHOW_FAN]);
                        if( is_running )
                            vv_reset_duty();
                    } else if( disp == VV_SHOW_RH )
                        disp = VV_SHOW_ON;
                    else
                        disp++;
                break;
            }

            ESP_LOGI(TAG, "%d button touched. disp: %d, is_edit: %d, is_running: %d "
                    , btn, disp, is_edit, is_running);

            vv_disp_set(disp);

        } else if( btn != VV_BTN_NONE)
        {
            ESP_LOGI( TAG, "Button hold %d. Level: %d"
                         , btn, gpio_get_level(btn));
            if( !gpio_get_level(btn) )
            {
                // TODO: change delay to 300000ms
                delay = UPDATE_DELAY;
                btn   = VV_BTN_NONE;
                continue;
            }

            switch( btn )
            {
                case VV_BTN_MAIN:
                    if( is_edit )
                    {
                        vv_save_duty();
                        vv_disp_freeze(0);

                        is_edit = 0;
                    } else if( disp == VV_SHOW_ON )
                    {
                        vv_set_running(is_running ? 0 : 1);
                    } else if( disp == VV_SHOW_FAN || disp == VV_SHOW_PUMP )
                    {
                        vv_disp_freeze(1);

                        is_edit = 1;
                    }
                    delay = UPDATE_DELAY;
                    btn   = VV_BTN_NONE;
                break;
                case VV_BTN_LEFT:
                    if( is_edit ) 
                        vv_change_duty(disp - VV_SHOW_FAN, -5);
                    if( is_running )
                        vv_reset_duty();
                    vv_disp_update(disp, vv_duty[disp-VV_SHOW_FAN]);
                break;
                case VV_BTN_RIGHT:
                    if( is_edit ) 
                        vv_change_duty(disp - VV_SHOW_FAN, +5);
                    if( is_running )
                        vv_reset_duty();
                    vv_disp_update(disp, vv_duty[disp-VV_SHOW_FAN]);
                break;
            }
            vv_disp_set(disp);
        } else if( me_bme280_read(bme280_conf, &bme280_data) == ESP_OK )
        {
            delay = UPDATE_DELAY;
            ESP_LOGI(TAG, "BME update. Temp: %d, Humid: %d."
                        , bme280_data.temperature
                        , bme280_data.humidity);
            vv_disp_update(VV_SHOW_TEMP, bme280_data.temperature/100);
            vv_disp_update(VV_SHOW_RH, bme280_data.humidity/1024);
        }
    }
}

void gorynych(void)
{
    esp_err_t       err;
    gpio_config_t   io_conf = {
        .intr_type      = GPIO_PIN_INTR_POSEDGE,
        .mode           = GPIO_MODE_INPUT,
        .pull_up_en     = 0,
        .pull_down_en   = 1,
        .pin_bit_mask   = (   (1ULL<<VV_BTN_MAIN) 
                            | (1ULL<<VV_BTN_LEFT)
                            | (1ULL<<VV_BTN_RIGHT)
                          )
    };

    err = nvs_flash_init();
    if( err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND )
    {
      ESP_ERROR_CHECK(nvs_flash_erase());
      err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(me_i2c_master_init(I2C_NUM_0
                            , VV_SDA_PIN, VV_SCL_PIN
                            , 100000, ME_I2C_PULLUP_ENABLE));

    me_bme280_init(bme280_conf);
    
    vv_duty_init();
    vv_disp_init();
    
    vv_restore_duty();
    vv_disp_update(VV_SHOW_FAN, vv_duty[VV_IDX_FAN]);
    vv_disp_update(VV_SHOW_PUMP, vv_duty[VV_IDX_PUMP]);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK( mdns_init() );
    ESP_ERROR_CHECK( mdns_hostname_set("gorinych") );
    ESP_ERROR_CHECK( mdns_instance_name_set("Gorinych") );
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    mdns_service_instance_name_set("_http", "_tcp", "Gorinych");

    
    queue = xQueueCreate(5, sizeof(uint32_t));

    xTaskCreate(vvowen_task, "vvowen_task", 2048, NULL, 10, NULL);

    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(VV_BTN_MAIN, btn_handler, (void*) VV_BTN_MAIN);
    gpio_isr_handler_add(VV_BTN_LEFT, btn_handler, (void*) VV_BTN_LEFT);
    gpio_isr_handler_add(VV_BTN_RIGHT, btn_handler, (void*) VV_BTN_RIGHT);
    
    ESP_LOGI(TAG, "Buttons initialized. Levels: main=%d, left=%d, right=%d"
                , gpio_get_level(VV_BTN_MAIN)
                , gpio_get_level(VV_BTN_LEFT)
                , gpio_get_level(VV_BTN_RIGHT)
                );

    me_wifi_ap_init();
    vv_http_init();

    ESP_LOGI(TAG, "VV initialized");
}
