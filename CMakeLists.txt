idf_component_register(SRCS "gorynych.c" "vvduty.c" "vvdisplay.c" "vvhttp.c"
                    INCLUDE_DIRS "include"
                    REQUIRES "max7219 nvs_flash mdns esp_http_server me_debug me_bme280 me_wifi_ap")
