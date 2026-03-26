/**
 * @file    esp_now_receive.h
 * @brief   ESP-NOW 接收任务
 * 
 * 职责：
 * - 注册 ESP-NOW 接收回调
 * - 解析命令并更新设备标志位
 */
#ifndef ESP_NOW_RECEIVE_H
#define ESP_NOW_RECEIVE_H

#include <stdint.h>

void EspNow_Receive_Init(void);

#endif
