idf_component_register(SRCS "iot_slave_main.c"
                            "APP/Src/esp_now_receive.c"
                            "APP/Src/esp_now_heartbeat.c"
                            "APP/Src/iot_control_task.c"
                            "APP/Src/sensor_adc_task.c"
                            "APP/Src/app_wifi.c"
                            "BSP/Src/bsp_wifi.c"
                       PRIV_REQUIRES esp_wifi nvs_flash driver esp_netif esp_event
                       INCLUDE_DIRS "." "APP/Inc" "BSP/Inc")
