/**
 * @file    esp_now_receive.c
 * @brief   ESP-NOW 接收任务 (一楼设备)
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
static uint8_t g_espnow_channel = 0;

/* ================= 协议定义（与主机保持一致） ================= */
typedef struct {
    uint8_t command;
    uint8_t device_id;
    uint8_t gpio_index;
    uint8_t value;
    uint8_t reserved[4];
} iot_command_packet_t;

typedef struct {
    uint8_t command;
    uint8_t device_id;
    uint8_t gpio_index;
    uint8_t value;
    char device_name[24];
} iot_announce_v2_packet_t;

typedef enum {
    IOT_CMD_SET_GPIO = 0x01,
    IOT_CMD_GET_GPIO = 0x02,
    IOT_CMD_GET_ALL_GPIO = 0x03,
    IOT_CMD_HEARTBEAT = 0x04,
    IOT_CMD_RESPONSE = 0x05,
    IOT_CMD_DISCOVER = 0x06,
    IOT_CMD_ANNOUNCE = 0x07,
    IOT_CMD_ANNOUNCE_V2 = 0x08,
    IOT_CMD_SET_SERVO = 0x10,
    IOT_CMD_SET_LIGHT = 0x11,
    IOT_CMD_SET_RELAY = 0x12,
    IOT_CMD_BROADCAST_ALL_OFF = 0x30,
    IOT_CMD_BROADCAST_ALL_ON = 0x31
} iot_command_t;

/* ================= 设备名称定义 ================= */
#define DEVICE_NAME "三楼设备"
#define TOTAL_GPIO_COUNT (LIGHT_COUNT + RELAY_COUNT + SERVO_COUNT)

/* ================= 响应发送 ================= */
static uint8_t Get_Current_Wifi_Channel(void) {
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        return ap_info.primary;
    }
    return ESPNOW_CHANNEL;
}

static void Send_Response(const uint8_t* dest_mac, uint8_t cmd, uint8_t gpio_index, uint8_t value) {
    iot_command_packet_t resp = {
        .command = cmd,
        .device_id = 0,
        .gpio_index = gpio_index,
        .value = value,
        .reserved = {0}
    };

    if (dest_mac[0] == 0xFF && dest_mac[1] == 0xFF && dest_mac[2] == 0xFF &&
        dest_mac[3] == 0xFF && dest_mac[4] == 0xFF && dest_mac[5] == 0xFF) {
        esp_now_send(dest_mac, (const uint8_t*)&resp, sizeof(resp));
    } else {
        uint8_t channel = Get_Current_Wifi_Channel();
        esp_now_peer_info_t peer = {
            .channel = channel,
            .ifidx = WIFI_IF_STA,
            .encrypt = false,
        };
        memcpy(peer.peer_addr, dest_mac, 6);
        if (esp_now_is_peer_exist(dest_mac)) {
            esp_now_mod_peer(&peer);
        } else {
            esp_now_add_peer(&peer);
        }
        esp_now_send(dest_mac, (const uint8_t*)&resp, sizeof(resp));
    }
    ESP_LOGD(TAG, "Response sent: cmd=%d, gpio=%d, value=%d", cmd, gpio_index, value);
}

static void Send_Announce(const uint8_t* dest_mac) {
    iot_announce_v2_packet_t resp = {
        .command = IOT_CMD_ANNOUNCE_V2,
        .device_id = 0,
        .gpio_index = TOTAL_GPIO_COUNT,
        .value = 1,
        .device_name = DEVICE_NAME
    };

    uint8_t channel = Get_Current_Wifi_Channel();
    esp_now_peer_info_t peer = {
        .channel = channel,
        .ifidx = WIFI_IF_STA,
        .encrypt = false,
    };
    memcpy(peer.peer_addr, dest_mac, 6);
    if (esp_now_is_peer_exist(dest_mac)) {
        esp_now_mod_peer(&peer);
    } else {
        esp_now_add_peer(&peer);
    }
    esp_now_send(dest_mac, (const uint8_t*)&resp, sizeof(resp));
    ESP_LOGI(TAG, "Announce V2 sent: %s (channel %d)", DEVICE_NAME, channel);
}

/* ================= 命令处理（只更新标志位） ================= */
static void Process_Command(const uint8_t* src_mac, const iot_command_packet_t* cmd) {
    ESP_LOGI(TAG, "Command: cmd=%d, gpio=%d, value=%d",
             cmd->command, cmd->gpio_index, cmd->value);

    switch (cmd->command) {
        case IOT_CMD_SET_GPIO:
            if (cmd->gpio_index < LIGHT_COUNT) {
                g_device_flags.light[cmd->gpio_index] = (cmd->value == 1) ? ON : OFF;
                ESP_LOGI(TAG, "Light[%d] = %s", cmd->gpio_index, cmd->value ? "ON" : "OFF");
                Send_Response(src_mac, IOT_CMD_RESPONSE, cmd->gpio_index, cmd->value);
            } else if (cmd->gpio_index < LIGHT_COUNT + RELAY_COUNT) {
                g_device_flags.relay[cmd->gpio_index - LIGHT_COUNT] = (cmd->value == 1) ? ON : OFF;
                ESP_LOGI(TAG, "Relay[%d] = %s", cmd->gpio_index - LIGHT_COUNT, cmd->value ? "ON" : "OFF");
                Send_Response(src_mac, IOT_CMD_RESPONSE, cmd->gpio_index, cmd->value);
            }
            break;

        case IOT_CMD_SET_LIGHT:
            if (cmd->gpio_index < LIGHT_COUNT) {
                g_device_flags.light[cmd->gpio_index] = (cmd->value == 1) ? ON : OFF;
                ESP_LOGI(TAG, "Light[%d] = %s", cmd->gpio_index, cmd->value ? "ON" : "OFF");
                Send_Response(src_mac, IOT_CMD_RESPONSE, cmd->gpio_index, cmd->value);
            }
            break;

        case IOT_CMD_SET_RELAY:
            if (cmd->gpio_index < RELAY_COUNT) {
                g_device_flags.relay[cmd->gpio_index] = (cmd->value == 1) ? ON : OFF;
                ESP_LOGI(TAG, "Relay[%d] = %s", cmd->gpio_index, cmd->value ? "ON" : "OFF");
                Send_Response(src_mac, IOT_CMD_RESPONSE, cmd->gpio_index, cmd->value);
            }
            break;

        case IOT_CMD_SET_SERVO:
            if (cmd->gpio_index < SERVO_COUNT && cmd->value <= SERVO_135) {
                g_device_flags.servo[cmd->gpio_index] = (servo_angle_enum_t)cmd->value;
                ESP_LOGI(TAG, "Servo[%d] = %d deg", cmd->gpio_index, cmd->value);
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

        case IOT_CMD_BROADCAST_ALL_OFF:
            for (int i = 0; i < LIGHT_COUNT; i++) g_device_flags.light[i] = OFF;
            for (int i = 0; i < RELAY_COUNT; i++) g_device_flags.relay[i] = OFF;
            for (int i = 0; i < SERVO_COUNT; i++) g_device_flags.servo[i] = SERVO_0;
            ESP_LOGI(TAG, "All devices OFF");
            break;

        case IOT_CMD_BROADCAST_ALL_ON:
            for (int i = 0; i < LIGHT_COUNT; i++) g_device_flags.light[i] = ON;
            for (int i = 0; i < RELAY_COUNT; i++) g_device_flags.relay[i] = ON;
            for (int i = 0; i < SERVO_COUNT; i++) g_device_flags.servo[i] = SERVO_90;
            ESP_LOGI(TAG, "All devices ON");
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
    if (tx_info == NULL) {
        return;
    }
    ESP_LOGD(TAG, "ESP-NOW Send status: %s", status == ESP_NOW_SEND_SUCCESS ? "SUCCESS" : "FAIL");
}

/* ================= 初始化 ================= */
void EspNow_Receive_Init(void) {
    ESP_LOGI(TAG, "Initializing ESP-NOW...");

    wifi_country_t country = {
        .cc = "CN",
        .schan = 1,
        .nchan = 13,
        .policy = WIFI_COUNTRY_POLICY_AUTO
    };
    ESP_ERROR_CHECK(esp_wifi_set_country(&country));

    ESP_ERROR_CHECK(esp_now_init());

    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        g_espnow_channel = ap_info.primary;
        ESP_LOGI(TAG, "WiFi connected, ESP-NOW will use channel %d", g_espnow_channel);
    } else {
        g_espnow_channel = ESPNOW_CHANNEL;
        ESP_LOGW(TAG, "WiFi not connected yet, using default channel %d", g_espnow_channel);
    }

    esp_now_register_recv_cb(EspNow_Recv_Callback);
    esp_now_register_send_cb(EspNow_Send_Callback);

    ESP_LOGI(TAG, "ESP-NOW initialized");
}
