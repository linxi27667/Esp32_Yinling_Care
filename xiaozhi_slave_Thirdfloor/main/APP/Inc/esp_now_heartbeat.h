/**
 * @file    esp_now_heartbeat.h
 * @brief   ESP-NOW 心跳任务
 * 
 * 职责：
 * - 定时发送心跳包给主机
 * - 维护与主机的连接状态
 */
#ifndef ESP_NOW_HEARTBEAT_H
#define ESP_NOW_HEARTBEAT_H

void EspNow_Heartbeat_Init(void);

#endif
