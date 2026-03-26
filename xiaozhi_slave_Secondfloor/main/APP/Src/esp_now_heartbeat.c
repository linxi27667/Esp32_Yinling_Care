/**
 * @file    esp_now_heartbeat.c
 * @brief   ESP-NOW 心跳任务
 * 
 * 职责：
 * - 定时发送心跳包给主机
 * - 维护与主机的连接状态
 * 
 * 架构原则：
 * - 独立任务，专注心跳
 * - 不操作任何 GPIO，只负责通信
 */
#include "esp_now_heartbeat.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "ESP_NOW_HB";

#define HEARTBEAT_INTERVAL_MS   30000

typedef struct {
    uint8_t command;
    uint8_t device_id;
    uint8_t gpio_index;
    uint8_t value;
    uint8_t reserved[4];
} __attribute__((packed)) iot_command_packet_t;

#define IOT_CMD_HEARTBEAT    0x04

static uint8_t g_master_mac[6] = {0};
static bool g_master_known = false;
static uint8_t g_espnow_channel = 1;

void Heartbeat_Task(void* arg) {
    vTaskDelay(pdMS_TO_TICKS(5000));

    while (1) {
        if (g_master_known) {
            if (!esp_now_is_peer_exist(g_master_mac)) {
                esp_now_peer_info_t peer = {0};
                memcpy(peer.peer_addr, g_master_mac, 6);
                peer.channel = g_espnow_channel;
                peer.ifidx = WIFI_IF_STA;
                peer.encrypt = false;
                esp_now_add_peer(&peer);
                ESP_LOGI(TAG, "Re-added master as peer");
            }

            iot_command_packet_t heartbeat = {
                .command = IOT_CMD_HEARTBEAT,
                .device_id = 0,
                .gpio_index = 0,
                .value = 0,
                .reserved = {0}
            };
            esp_now_send(g_master_mac, (const uint8_t*)&heartbeat, sizeof(heartbeat));
            ESP_LOGD(TAG, "Heartbeat sent to master");
        }

        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS));
    }
}

void EspNow_Heartbeat_Init(void) {
    BaseType_t ret = xTaskCreate(
        Heartbeat_Task,
        "esp_now_hb",
        2048,
        NULL,
        4,
        NULL
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create heartbeat task");
    } else {
        ESP_LOGI(TAG, "Heartbeat task started (interval: %d ms)", HEARTBEAT_INTERVAL_MS);
    }
}
