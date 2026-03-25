/**
 * @file    esp_now_receive.c
 * @brief   ESP-NOW 接收任务
 * 
 * 职责：
 * - 注册 ESP-NOW 接收回调
 * - 解析命令并更新设备标志位
 * 
 * 架构原则：
 * - 只负责接收和解析，不直接操作硬件
 * - 通过更新全局标志位，由 iot_control_task 刷新 GPIO
 */
#include "esp_now_receive.h"
#include "iot_control_task.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_mac.h"
#include "driver/gpio.h"

static const char* TAG = "ESP_NOW_RECV";

#define ESPNOW_CHANNEL  1

/* ================= 主机 MAC 管理 ================= */
static uint8_t g_master_mac[6] = {0};
static bool g_master_known = false;
static uint8_t g_espnow_channel = ESPNOW_CHANNEL;

/* ================= 协议定义（与主机保持一致） ================= */
typedef struct {
    uint8_t command;
    uint8_t device_id;
    uint8_t gpio_index;
    uint8_t value;
    uint8_t reserved[4];
} iot_command_packet_t;

typedef enum {
    IOT_CMD_SET_GPIO = 0x01,
    IOT_CMD_GET_GPIO = 0x02,
    IOT_CMD_GET_ALL_GPIO = 0x03,
    IOT_CMD_HEARTBEAT = 0x04,
    IOT_CMD_RESPONSE = 0x05,
    IOT_CMD_DISCOVER = 0x06,
    IOT_CMD_ANNOUNCE = 0x07,
    IOT_CMD_SET_SERVO = 0x10,
    IOT_CMD_OPEN_MEDICINE_BOX = 0x20,
    IOT_CMD_BROADCAST_ALL_OFF = 0x30,
    IOT_CMD_BROADCAST_ALL_ON = 0x31
} iot_command_t;

/* ================= 响应发送 ================= */
static void Send_Response(const uint8_t* dest_mac, uint8_t cmd, uint8_t gpio_index, uint8_t value) {
    iot_command_packet_t resp = {
        .command = cmd,
        .device_id = 0,
        .gpio_index = gpio_index,
        .value = value,
        .reserved = {0}
    };

    if (!esp_now_is_peer_exist(dest_mac)) {
        esp_now_peer_info_t peer = {0};
        memcpy(peer.peer_addr, dest_mac, 6);
        peer.channel = g_espnow_channel;
        peer.ifidx = WIFI_IF_STA;
        peer.encrypt = false;
        esp_now_add_peer(&peer);
    }

    esp_now_send(dest_mac, (const uint8_t*)&resp, sizeof(resp));
    ESP_LOGD(TAG, "Response sent: cmd=%d, gpio=%d, value=%d", cmd, gpio_index, value);
}

static void Send_Announce(const uint8_t* dest_mac) {
    iot_command_packet_t resp = {
        .command = IOT_CMD_ANNOUNCE,
        .device_id = 0,
        .gpio_index = 4,
        .value = 1,
        .reserved = {0}
    };

    if (!esp_now_is_peer_exist(dest_mac)) {
        esp_now_peer_info_t peer = {0};
        memcpy(peer.peer_addr, dest_mac, 6);
        peer.channel = g_espnow_channel;
        peer.ifidx = WIFI_IF_STA;
        peer.encrypt = false;
        esp_now_add_peer(&peer);
    }

    esp_now_send(dest_mac, (const uint8_t*)&resp, sizeof(resp));
    ESP_LOGI(TAG, "Announce sent to master");
}

/* ================= 命令处理（只更新标志位） ================= */
static void Process_Command(const uint8_t* src_mac, const iot_command_packet_t* cmd) {
    ESP_LOGI(TAG, "Command: cmd=%d, gpio=%d, value=%d",
             cmd->command, cmd->gpio_index, cmd->value);

    switch (cmd->command) {
        case IOT_CMD_SET_GPIO:
            if (cmd->gpio_index < MAX_GPIOS) {
                if (cmd->gpio_index == 0) {
                    g_device_flags.light_state = (cmd->value == 1) ? LIGHT_ON : LIGHT_OFF;
                }
                ESP_LOGI(TAG, "GPIO[%d] = %d", cmd->gpio_index, cmd->value);
                Send_Response(src_mac, IOT_CMD_RESPONSE, cmd->gpio_index, cmd->value);
            }
            break;

        case IOT_CMD_GET_GPIO:
            ESP_LOGI(TAG, "Get GPIO[%d]", cmd->gpio_index);
            break;

        case IOT_CMD_GET_ALL_GPIO:
            ESP_LOGI(TAG, "Get all GPIOs");
            break;

        case IOT_CMD_HEARTBEAT:
            Send_Response(src_mac, IOT_CMD_RESPONSE, 0, 1);
            break;

        case IOT_CMD_DISCOVER:
            memcpy(g_master_mac, src_mac, 6);
            g_master_known = true;
            ESP_LOGI(TAG, "Master discovered: %02X:%02X:%02X:%02X:%02X:%02X",
                     src_mac[0], src_mac[1], src_mac[2],
                     src_mac[3], src_mac[4], src_mac[5]);
            Send_Announce(src_mac);
            break;

        case IOT_CMD_SET_SERVO:
            if (cmd->value <= MED_BOX_6) {
                g_device_flags.box_state = (med_box_state_enum_t)cmd->value;
                ESP_LOGI(TAG, "Servo set to box_state=%d", cmd->value);
                Send_Response(src_mac, IOT_CMD_RESPONSE, 0xFE, cmd->value);
            }
            break;

        case IOT_CMD_OPEN_MEDICINE_BOX:
            if (cmd->value <= MED_BOX_6) {
                ESP_LOGI(TAG, "Open medicine box %d", cmd->value);
                g_device_flags.box_state = (med_box_state_enum_t)cmd->value;
                Send_Response(src_mac, IOT_CMD_RESPONSE, 0xFE, cmd->value);
            }
            break;

        case IOT_CMD_BROADCAST_ALL_OFF:
            g_device_flags.light_state = LIGHT_OFF;
            g_device_flags.box_state = MED_BOX_CLOSE;
            ESP_LOGI(TAG, "All devices OFF");
            break;

        case IOT_CMD_BROADCAST_ALL_ON:
            g_device_flags.light_state = LIGHT_ON;
            g_device_flags.box_state = MED_BOX_1;
            ESP_LOGI(TAG, "All devices ON (home mode)");
            break;

        default:
            ESP_LOGW(TAG, "Unknown command: %d", cmd->command);
            break;
    }
}

/* ================= ESP-NOW 回调 ================= */
static void EspNow_Recv_Callback(const esp_now_recv_info_t* recv_info,
                                  const uint8_t* data, int len) {
    if (recv_info == NULL || recv_info->src_addr == NULL || data == NULL) {
        return;
    }

    if (len < sizeof(iot_command_packet_t)) {
        ESP_LOGW(TAG, "Short packet: %d bytes", len);
        return;
    }

    const uint8_t* src_mac = recv_info->src_addr;
    const iot_command_packet_t* cmd = (const iot_command_packet_t*)data;

    Process_Command(src_mac, cmd);
}

static void EspNow_Send_Callback(const esp_now_send_info_t* tx_info,
                                  esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGD(TAG, "Send success");
    } else {
        ESP_LOGW(TAG, "Send failed");
    }
}

/* ================= 初始化 ================= */
static void EspNow_Init(void) {
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(EspNow_Recv_Callback));
    ESP_ERROR_CHECK(esp_now_register_send_cb(EspNow_Send_Callback));

    wifi_ap_record_t ap_info;
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
    if (ret == ESP_OK) {
        g_espnow_channel = ap_info.primary;
        ESP_LOGI(TAG, "WiFi Channel: %d", ap_info.primary);
    } else {
        g_espnow_channel = ESPNOW_CHANNEL;
    }

    esp_now_peer_info_t peer = {
        .channel = g_espnow_channel,
        .ifidx = WIFI_IF_STA,
        .encrypt = false,
    };
    memcpy(peer.peer_addr, (uint8_t[]){0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 6);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    ESP_LOGI(TAG, "ESP-NOW initialized (channel %d)", g_espnow_channel);
}

static void Print_Mac(void) {
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    ESP_LOGI(TAG, "Device MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void EspNow_Receive_Init(void) {
    Print_Mac();
    EspNow_Init();
}
