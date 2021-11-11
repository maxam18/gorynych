/* Vovan's oil heater. HTTP server
 * File: vvhttp.c
 * Started: Sat Oct 30 18:16:21 MSK 2021
 * Author: Max Amzarakov (maxam18 _at_ gmail _._ com)
 * Copyright (c) 2021 ..."
 */
#include <string.h>
#include <stdlib.h>

#include "sdkconfig.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_wifi.h"

#include "esp_netif.h"
#include "mdns.h"

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include <esp_http_server.h>

#include "vvduty.h"
#include "vvdisplay.h"
#include "gorynych.h"

static const char           *TAG = "HTTP";

static size_t make_rep(char *buf, size_t len)
{
    static const char *status[] = { "stop", "run", "fail" };
    return snprintf(buf, len
                        , "{ \"fan\": %d, \"pump\": %d, \"temp\": %2.2f, \"rh\": %2.2f, status: \"%s\" }"
                        , vv_duty[VV_IDX_FAN], vv_duty[VV_IDX_PUMP]
                        , (double)bme280_data.temperature/100.0
                        , (double)bme280_data.humidity/1024.0
                        , vv_get_running() ? status[1] : status[0]
                        );
}

static esp_err_t json_get_handler(httpd_req_t *req)
{
    char     resp[512];
    size_t   len;

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "null");

    len = make_rep(resp, sizeof(resp));
    
    httpd_resp_send(req, resp, len);

    return ESP_OK;
}


static inline uint8_t s22u(const u_char *p)
{
    u_char ch;
    uint8_t i = 0;

    ch = *p++;
    if( ch >= '0' && ch <= '9' )
        i = ch - '0';
    ch = *p;
    if( ch >= '0' && ch <= '9' )
        i = i * 10 + ch - '0';

    return i;
}

static esp_err_t control_get_handler(httpd_req_t *req)
{
    char     resp[512];
    size_t   len;
    u_char   *p;

    p = (u_char *)req->uri + sizeof("/control/.");
    switch( *p )
    {
        case 'o':
            vv_set_running(0);
        break;
        case 'a':
            vv_set_running(1);
        break;
        case 'm':
            vv_duty[VV_IDX_PUMP] = s22u(p+3);
            vv_start_duty();
        break;
        case 'n':
            vv_duty[VV_IDX_FAN] = s22u(p+2);
            vv_start_duty();
        break;
    }

    len = make_rep(resp, sizeof(resp));

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "null");

    httpd_resp_send(req, resp, len);

    return ESP_OK;
}

static const httpd_uri_t uri_conf_json = {
    .uri       = "/json",
    .method    = HTTP_GET,
    .handler   = json_get_handler,
    .user_ctx  = NULL
};

/* /control/stop 
   /control/start 
   /control/pump/12
   /control/fan/22
 */
static const httpd_uri_t uri_conf_control = {
    .uri       = "/control/*",
    .method    = HTTP_GET,
    .handler   = control_get_handler,
    .user_ctx  = NULL
};

esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Only /json and /control URI is available");

    return ESP_FAIL;
}

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);

    if( httpd_start(&server, &config) == ESP_OK )
    {
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &uri_conf_json);
        httpd_register_uri_handler(server, &uri_conf_control);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static void disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver");
        httpd_stop(*server);
        *server = NULL;
    }
}

static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}

void vv_http_init(void)
{
    static httpd_handle_t server = NULL;

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));

    server = start_webserver();
}


