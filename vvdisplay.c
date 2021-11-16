/* Vovan's oil heater. Display
 * File: vvdisplay.c
 * Started: Thu Oct 28 19:50:52 MSK 2021
 * Author: Max Amzarakov (maxam18 _at_ gmail _._ com)
 * Copyright (c) 2021 ..."
 */

#include <stdlib.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <max7219.h>

#include "font8x8_128LC.h"

#include <me_debug.h>
#include <gorynych.h>
#include <vvduty.h>
#include <vvdisplay.h>

#ifndef APP_CPU_NUM
#define APP_CPU_NUM PRO_CPU_NUM
#endif

#define SCROLL_DELAY pdMS_TO_TICKS(100)
#define CASCADE_SIZE 3

#define HOST HSPI_HOST

#define PIN_NUM_MOSI 19
#define PIN_NUM_CLK  18
#define PIN_NUM_CS   23

#define VV_BRIGHT_BLINK   16
#define VV_BRIGHT_DIM     2 
#define VV_BRIGHT_DEF     12

typedef struct {
    uint8_t     *start;
    uint8_t     *last;
    uint8_t     *pos;
    uint8_t      freeze;
    uint8_t      hold;
} vv_dtext_t;

static vv_dtext_t        dtext;

#define DISP_FONT        font8x8_128LC
#define DISP_DEF_TEXT    "Off B00 H00 T00 R00   "
// VV_SHOW_ MIN, ON, FAN, PUMP, TEMP, RH, MAX
static uint8_t           dtext_shift[] = {
    0, 0, 4*8, 8*8, 12*8, 16*8, 16*8
};

static void text_init(const char *text)
{
    uint8_t ch, *p, len = strlen(text);

    printf("Text to initialize: (len: %d) '%s' \n", len, text);
    dtext.start = dtext.pos = malloc(len*8);
    dtext.last  = dtext.start + 8*(len - CASCADE_SIZE);
    
    p = dtext.start;
    while( len-- > 0 )
    {
        ch = *text++;
        memcpy(p, DISP_FONT[ch], 8);
        p += 8;
    }
    dtext.freeze = dtext.hold = 0;

    printf("Dtext: %u, end: %u\n", (uint32_t)dtext.start, (uint32_t)dtext.last);
}

void vv_disp_freeze(uint8_t sw)
{
    dtext.freeze = sw;
}

void vv_disp_update_run(uint8_t is_running)
{
    uint8_t *p = dtext.start + 8;
    if( is_running ) 
    {
        memcpy(p, DISP_FONT['n'], 8);
        memcpy(p+8, DISP_FONT[' '], 8);
    } else {
        memcpy(p, DISP_FONT['f'], 8);
        memcpy(p+8, DISP_FONT['f'], 8);
    }
}

void vv_disp_update(vv_disp_enum_t what, int8_t val)
{
    uint8_t     *p = dtext.start + dtext_shift[what] + 8;

me_debug("DISP", "Updating %d with %d", what, val);
    memcpy(p, DISP_FONT['0'+val/10], 8);
    memcpy(p+8, DISP_FONT['0'+val%10], 8);
}

void vv_disp_set(vv_disp_enum_t what)
{
    dtext.pos = dtext.start + dtext_shift[what];
    dtext.hold = 50;
}

static void display_task(void *ign)
{
    uint8_t         *p;
    static max7219_t dev = {
        .cascade_size = CASCADE_SIZE,
        .digits = 0,
        .mirrored = true
    };
    // Configure device
    ESP_ERROR_CHECK(max7219_init_desc(&dev, HOST, PIN_NUM_CS));
    ESP_ERROR_CHECK(max7219_init(&dev));

    max7219_set_brightness(&dev, VV_BRIGHT_DEF);

    while( 1 )
    {

        p = dtext.pos;
        for( uint8_t c = 0; c < CASCADE_SIZE; c++ )
            max7219_draw_image_8x8(&dev, c * 8, p + c * 8);

        if( dtext.freeze )
        {
            max7219_set_brightness(&dev, VV_BRIGHT_DIM);
            vTaskDelay(SCROLL_DELAY);
            max7219_set_brightness(&dev, VV_BRIGHT_DEF);

            continue;
        }

        if( dtext.hold )
        {
            dtext.hold--;
        } else if( ++dtext.pos > dtext.last )
            dtext.pos = dtext.start;

        vTaskDelay(SCROLL_DELAY);
    }
}

void vv_disp_init()
{
    // Configure SPI bus
    spi_bus_config_t cfg = {
       .mosi_io_num = PIN_NUM_MOSI,
       .miso_io_num = -1,
       .sclk_io_num = PIN_NUM_CLK,
       .quadwp_io_num = -1,
       .quadhd_io_num = -1,
       .max_transfer_sz = 0,
       .flags = 0
    };
    ESP_ERROR_CHECK(spi_bus_initialize(HOST, &cfg, 1));

    text_init(DISP_DEF_TEXT);

    xTaskCreate(display_task, "display_task", 2048, NULL, 10, NULL);

}
