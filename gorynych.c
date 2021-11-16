/* Vovan's oil heater. Main prog
 * File: gorynych.c
 * Started: Thu Oct 28 19:50:12 MSK 2021
 * Author: Max Amzarakov (maxam18 _at_ gmail _._ com)
 * Copyright (c) 2021 ..."
 */

#include <string.h>
#include "gorynych.h"

#include "esp_system.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/mcpwm.h"

#include "esp_netif.h"
#include "mdns.h"

#include <me_wifi_ap.h>

#include <vvduty.h>
#include <vvhttp.h>
#include <vvdisplay.h>

#include <encoder.h>

#define VV_BTN_NONE        0
#define VV_RE_BTN          34
#define VV_RE_PINA         35
#define VV_RE_PINB         33

#define VV_SDA_PIN         21
#define VV_SCL_PIN         22

#define HOLD_DELAY         pdMS_TO_TICKS(1500)
#define UPDATE_DELAY       pdMS_TO_TICKS(15000)

static me_bme280_conf_t     bme280_conf[1] = {{ 
                                      .port = I2C_NUM_0
                                    , .addr = ME_BME280_I2C_ADDR_PRIMARY
                                }};
me_bme280_readings_t        bme280_data;
static const char          *TAG = "VV";
static QueueHandle_t        queue;
static rotary_encoder_t     re;

static void vvowen_task(void *ign)
{
    vv_disp_enum_t         disp = VV_SHOW_TEMP;
    uint8_t                is_edit = 0;
    rotary_encoder_event_t e;

    while( 1 )
    {
        if( xQueueReceive(queue, &e, UPDATE_DELAY) )
        {
            switch( e.type )
            {
                case RE_ET_BTN_PRESSED:
                    if( is_edit )
                    {
                        is_edit = 0;
                        vv_disp_freeze(0);
                        vv_duty_save();
                    }
                    ESP_LOGI(TAG, "Button pressed");
                    break;
                case RE_ET_BTN_RELEASED:
                    ESP_LOGI(TAG, "Button released");
                    break;
                case RE_ET_BTN_CLICKED:
                    ESP_LOGI(TAG, "Button clicked");
                    break;
                case RE_ET_BTN_LONG_PRESSED:
                    if( is_edit )
                    {
                        is_edit = 0;
                        vv_disp_freeze(0);
                        vv_duty_save();
                    } else if( disp == VV_SHOW_FAN || disp == VV_SHOW_PUMP )
                    {
                        is_edit = 1;
                        vv_disp_freeze(1);
                    } else if( disp == VV_SHOW_ON )
                    {
                        if( vv_duty_run() )
                            vv_duty_stop();
                        else
                            vv_duty_start();
                        disp = VV_SHOW_ON;
                    }
                    vv_disp_update_run(vv_duty_run());
                    ESP_LOGI(TAG, "Button long pressed");
                    break;
                case RE_ET_CHANGED:
                    if( is_edit )
                    {
                        vv_change_duty(disp - VV_SHOW_FAN, e.diff);
                        vv_disp_update(disp, vv_duty[disp - VV_SHOW_FAN]);
                    } else
                        disp += e.diff > 0 ? 1 : -1;
                    ESP_LOGI(TAG, "Changed by %d", e.diff);
                    break;
                default:
                    break;
            }
            if( disp == VV_SHOW_MIN )
                disp = VV_SHOW_RH;
            else if( disp == VV_SHOW_MAX )
                disp = VV_SHOW_ON;
            vv_disp_set(disp);
        } else if( me_bme280_read(bme280_conf, &bme280_data) == ESP_OK )
        {
            ESP_LOGI(TAG, "BME update. Temp: %d, Humid: %d."
                        , bme280_data.temperature
                        , bme280_data.humidity);
            vv_disp_update(VV_SHOW_TEMP, bme280_data.temperature/100);
            vv_disp_update(VV_SHOW_RH, bme280_data.humidity/1024);
        }
        ESP_LOGI(TAG, "Disp: %d, is_edit: %d, is_running: %d"
                    , disp, is_edit, vv_duty_run());
    }
}

void gorynych(void)
{
    esp_err_t       err;

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

    err = me_bme280_init(bme280_conf);
    if( err != ESP_OK )
        ESP_LOGW(TAG, "Cannot initialize bme280");
    
    vv_duty_init();
    vv_duty_stop();
    vv_duty_restore();
    
    vv_disp_init();
    vv_disp_update(VV_SHOW_FAN, vv_duty[VV_IDX_FAN]);
    vv_disp_update(VV_SHOW_PUMP, vv_duty[VV_IDX_PUMP]);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK( mdns_init() );
    ESP_ERROR_CHECK( mdns_hostname_set("gorinych") );
    ESP_ERROR_CHECK( mdns_instance_name_set("Gorinych") );
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
    mdns_service_instance_name_set("_http", "_tcp", "Gorinych");
    
    queue = xQueueCreate(5, sizeof(rotary_encoder_event_t));

    ESP_ERROR_CHECK(rotary_encoder_init(queue));
    memset(&re, 0, sizeof(rotary_encoder_t));
    re.pin_a = VV_RE_PINA;
    re.pin_b = VV_RE_PINB;
    re.pin_btn = VV_RE_BTN;
    ESP_ERROR_CHECK(rotary_encoder_add(&re));

    xTaskCreate(vvowen_task, "vvowen_task", 2048, NULL, 10, NULL);


    me_wifi_ap_init();
    vv_http_init();

    ESP_LOGI(TAG, "VV initialized");
}
