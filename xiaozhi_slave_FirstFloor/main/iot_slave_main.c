/**
 * @file    iot_slave_main.c
 * @brief   IoT从机设备主程序 - 接收主机控制命令
 * 
 * 功能：
 * 1. 连接WiFi路由器（与主机同一网络）
 * 2. 初始化ESP-NOW通信
 * 3. 接收主机命令控制LED和舵机
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_mac.h"

#include "app_wifi.h"

#define CONFIG_DEBUG_BTN_ENABLED 1
#define ESPNOW_CHANNEL  1

static const char* TAG = "IOT_SLAVE";
static uint8_t g_espnow_channel = ESPNOW_CHANNEL;

/* ================= GPIO 配置 ================= */
#define LED_GPIO_1      GPIO_NUM_2
#define LED_GPIO_2      GPIO_NUM_4
#define LED_GPIO_3      GPIO_NUM_5
#define LED_GPIO_4      GPIO_NUM_48

#define SERVO_GPIO      GPIO_NUM_18
#define DEBUG_BTN_GPIO  GPIO_NUM_1
#define MAX_GPIOS       4
#define HEARTBEAT_INTERVAL_MS   30000

/* ================= 舵机配置 ================= */
#define SERVO_MIN_PULSEWIDTH_US 500
#define SERVO_MAX_PULSEWIDTH_US 2500
#define SERVO_MAX_ANGLE        135

/* ================= 药盒配置 ================= */
#define MEDICINE_BOX_COUNT       6
//efine BOX_SERVO_ANGLE_7      -135
#define BOX_SERVO_ANGLE_6      -110
#define BOX_SERVO_ANGLE_5      -81
#define BOX_SERVO_ANGLE_4       -45
#define BOX_SERVO_ANGLE_3       -8
#define BOX_SERVO_ANGLE_2       29
#define BOX_SERVO_ANGLE_1       63
#define BOX_SERVO_ANGLE_CLOSE        99

static const int16_t g_box_angles[MEDICINE_BOX_COUNT + 1] = {
    [0] = BOX_SERVO_ANGLE_CLOSE,
    [1] = BOX_SERVO_ANGLE_1,
    [2] = BOX_SERVO_ANGLE_2,
    [3] = BOX_SERVO_ANGLE_3,
    [4] = BOX_SERVO_ANGLE_4,
    [5] = BOX_SERVO_ANGLE_5,
    [6] = BOX_SERVO_ANGLE_6
};

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
    IOT_CMD_OPEN_MEDICINE_BOX = 0x20
} iot_command_t;

typedef struct {
    gpio_num_t pin;
    char name[16];
    uint8_t state;
} slave_gpio_t;

static slave_gpio_t g_gpios[MAX_GPIOS];
static uint8_t g_master_mac[6] = {0};
static bool g_master_known = false;
static uint8_t g_servo_angle = 90;
static uint8_t g_current_box = 0;

#ifdef CONFIG_DEBUG_BTN_ENABLED
static uint8_t g_debug_box_index = 0;
static uint8_t g_debug_btn_last_state = 1;
#endif

/* ================= 硬件底层函数 ================= */
static void HW_Gpio_Init(gpio_num_t pin, uint8_t default_level) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(pin, default_level);
}

static void HW_Gpio_Write(gpio_num_t pin, uint8_t level) {
    gpio_set_level(pin, level);
}

static uint8_t HW_Gpio_Read(gpio_num_t pin) {
    return gpio_get_level(pin);
}

/* ================= 舵机底层函数 ================= */
static void HW_Servo_Init(void) {
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_14_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 50,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer_conf);
    
    ledc_channel_config_t channel_conf = {
        .gpio_num = SERVO_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&channel_conf);
    
    ESP_LOGI(TAG, "Servo initialized on GPIO %d", SERVO_GPIO);
}

static void HW_Servo_Set_Angle(int16_t angle) {
    if (angle > 135) {
        angle = 135;
    } else if (angle < -135) {
        angle = -135;
    }

    float pulse_width = SERVO_MIN_PULSEWIDTH_US +
        (float)(angle + 135) * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) / (SERVO_MAX_ANGLE * 2);

    uint32_t duty = (uint32_t)((pulse_width * 16384) / 20000);

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);

    g_servo_angle = (uint8_t)angle;
    ESP_LOGI(TAG, "Servo angle set to %d degrees (pulse=%.1f us, duty=%lu)", angle, pulse_width, duty);
}

#ifdef CONFIG_DEBUG_BTN_ENABLED
static void Debug_Btn_Init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << DEBUG_BTN_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    ESP_LOGI(TAG, "Debug button initialized on GPIO %d (pull-up)", DEBUG_BTN_GPIO);
}

static void Debug_Btn_Task(void* arg) {
    while (1) {
        uint8_t btn_state = gpio_get_level(DEBUG_BTN_GPIO);
        ESP_LOGD(TAG, "Debug BTN: state=%d, last=%d", btn_state, g_debug_btn_last_state);
        if (btn_state == 0 && g_debug_btn_last_state == 1) {
            ESP_LOGI(TAG, "Debug BTN: pressed! current_index=%d", g_debug_box_index);
            g_debug_box_index = (g_debug_box_index + 1) % (MEDICINE_BOX_COUNT + 1);
            int16_t angle = g_box_angles[g_debug_box_index];
            HW_Servo_Set_Angle(angle);
            ESP_LOGI(TAG, "Debug: box %d -> angle %d", g_debug_box_index, angle);
            vTaskDelay(pdMS_TO_TICKS(300));
        }
        g_debug_btn_last_state = btn_state;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
#endif

/* ================= GPIO 初始化 ================= */
static void Init_Gpios(void) {
    g_gpios[0].pin = LED_GPIO_1;
    strcpy(g_gpios[0].name, "led1");
    g_gpios[0].state = 0;
    
    g_gpios[1].pin = LED_GPIO_2;
    strcpy(g_gpios[1].name, "led2");
    g_gpios[1].state = 0;
    
    g_gpios[2].pin = LED_GPIO_3;
    strcpy(g_gpios[2].name, "led3");
    g_gpios[2].state = 0;
    
    g_gpios[3].pin = LED_GPIO_4;
    strcpy(g_gpios[3].name, "led4");
    g_gpios[3].state = 0;
    
    for (int i = 0; i < MAX_GPIOS; i++) {
        HW_Gpio_Init(g_gpios[i].pin, 0);
        ESP_LOGI(TAG, "GPIO[%d]: %s -> GPIO%d", 
                 i, g_gpios[i].name, g_gpios[i].pin);
    }
}

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
        
        esp_err_t err = esp_now_add_peer(&peer);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to add peer: %s", esp_err_to_name(err));
        }
    }
    
    esp_err_t err = esp_now_send(dest_mac, (const uint8_t*)&resp, sizeof(resp));
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Response sent: cmd=%d, gpio=%d, value=%d", cmd, gpio_index, value);
    } else {
        ESP_LOGE(TAG, "Failed to send response: %s", esp_err_to_name(err));
    }
}

static void Send_Announce(const uint8_t* dest_mac) {
    iot_command_packet_t resp = {
        .command = IOT_CMD_ANNOUNCE,
        .device_id = 0,
        .gpio_index = MAX_GPIOS,
        .value = 0,
        .reserved = {0}
    };
    
    if (!esp_now_is_peer_exist(dest_mac)) {
        esp_now_peer_info_t peer = {0};
        memcpy(peer.peer_addr, dest_mac, 6);
        peer.channel = g_espnow_channel;
        peer.ifidx = WIFI_IF_STA;
        peer.encrypt = false;
        
        esp_err_t err = esp_now_add_peer(&peer);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to add master as peer: %s", esp_err_to_name(err));
            return;
        }
    }
    
    esp_err_t err = esp_now_send(dest_mac, (const uint8_t*)&resp, sizeof(resp));
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Announce sent to master %02X:%02X:%02X:%02X:%02X:%02X",
                 dest_mac[0], dest_mac[1], dest_mac[2], 
                 dest_mac[3], dest_mac[4], dest_mac[5]);
    } else {
        ESP_LOGE(TAG, "Failed to send announce: %s", esp_err_to_name(err));
    }
}

/* ================= 命令处理 ================= */
static void Process_Command(const uint8_t* src_mac, const iot_command_packet_t* cmd) {
    ESP_LOGI(TAG, "Received command: cmd=%d, gpio=%d, value=%d", 
             cmd->command, cmd->gpio_index, cmd->value);
    
    switch (cmd->command) {
        case IOT_CMD_SET_GPIO:
            if (cmd->gpio_index < MAX_GPIOS) {
                HW_Gpio_Write(g_gpios[cmd->gpio_index].pin, cmd->value);
                g_gpios[cmd->gpio_index].state = cmd->value;
                ESP_LOGI(TAG, "Set %s = %d", g_gpios[cmd->gpio_index].name, cmd->value);
                Send_Response(src_mac, IOT_CMD_RESPONSE, cmd->gpio_index, cmd->value);
            }
            break;
            
        case IOT_CMD_GET_GPIO:
            if (cmd->gpio_index < MAX_GPIOS) {
                uint8_t value = HW_Gpio_Read(g_gpios[cmd->gpio_index].pin);
                Send_Response(src_mac, IOT_CMD_RESPONSE, cmd->gpio_index, value);
            }
            break;
            
        case IOT_CMD_GET_ALL_GPIO: {
            uint8_t states = 0;
            for (int i = 0; i < MAX_GPIOS; i++) {
                if (g_gpios[i].state) {
                    states |= (1 << i);
                }
            }
            Send_Response(src_mac, IOT_CMD_RESPONSE, 0xFF, states);
            break;
        }
            
        case IOT_CMD_HEARTBEAT:
            Send_Response(src_mac, IOT_CMD_RESPONSE, 0, 1);
            break;
            
        case IOT_CMD_DISCOVER:
            memcpy(g_master_mac, src_mac, 6);
            g_master_known = true;
            ESP_LOGI(TAG, "Master discovered: %02X:%02X:%02X:%02X:%02X:%02X",
                     src_mac[0], src_mac[1], src_mac[2], src_mac[3], src_mac[4], src_mac[5]);
            Send_Announce(src_mac);
            break;
            
        case IOT_CMD_SET_SERVO:
            HW_Servo_Set_Angle(cmd->value);
            Send_Response(src_mac, IOT_CMD_RESPONSE, 0xFE, g_servo_angle);
            break;

        case IOT_CMD_OPEN_MEDICINE_BOX: {
            uint8_t box_index = cmd->value;
            if (box_index == 0 || box_index > MEDICINE_BOX_COUNT) {
                ESP_LOGW(TAG, "Invalid medicine box index: %d", box_index);
                HW_Servo_Set_Angle(BOX_SERVO_ANGLE_CLOSE);
                g_current_box = 0;
            } else {
                ESP_LOGI(TAG, "Opening medicine box %d", box_index);
                HW_Servo_Set_Angle(g_box_angles[box_index]);
                g_current_box = box_index;
            }
            Send_Response(src_mac, IOT_CMD_RESPONSE, 0xFE, g_current_box);
            break;
        }
            
        default:
            ESP_LOGW(TAG, "Unknown command: %d", cmd->command);
            break;
    }
}

/* ================= ESP-NOW 回调 ================= */
static void EspNow_Recv_Callback(const esp_now_recv_info_t* recv_info, const uint8_t* data, int len) {
    if (recv_info == NULL || recv_info->src_addr == NULL || data == NULL) {
        return;
    }
    
    if (len < sizeof(iot_command_packet_t)) {
        ESP_LOGW(TAG, "Received short packet: %d bytes", len);
        return;
    }
    
    const uint8_t* src_mac = recv_info->src_addr;
    const iot_command_packet_t* cmd = (const iot_command_packet_t*)data;
    
    Process_Command(src_mac, cmd);
}

static void EspNow_Send_Callback(const esp_now_send_info_t* tx_info, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGD(TAG, "Send success");
    } else {
        ESP_LOGW(TAG, "Send failed");
    }
}

/* ================= ESP-NOW 初始化 ================= */
static void EspNow_Init(void) {
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(EspNow_Recv_Callback));
    ESP_ERROR_CHECK(esp_now_register_send_cb(EspNow_Send_Callback));
    
    wifi_ap_record_t ap_info;
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
    if (ret == ESP_OK) {
        g_espnow_channel = ap_info.primary;
        ESP_LOGI(TAG, "Connected to AP: %s, Channel: %d", ap_info.ssid, ap_info.primary);
        
        esp_now_peer_info_t peer = {
            .channel = g_espnow_channel,
            .ifidx = WIFI_IF_STA,
            .encrypt = false,
        };
        memcpy(peer.peer_addr, (uint8_t[]){0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 6);
        ESP_ERROR_CHECK(esp_now_add_peer(&peer));
        
        ESP_LOGI(TAG, "ESP-NOW initialized (channel %d)", g_espnow_channel);
    } else {
        g_espnow_channel = ESPNOW_CHANNEL;
        ESP_LOGE(TAG, "Failed to get AP info, using default channel %d", ESPNOW_CHANNEL);
        
        esp_now_peer_info_t peer = {
            .channel = g_espnow_channel,
            .ifidx = WIFI_IF_STA,
            .encrypt = false,
        };
        memcpy(peer.peer_addr, (uint8_t[]){0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 6);
        ESP_ERROR_CHECK(esp_now_add_peer(&peer));
        
        ESP_LOGI(TAG, "ESP-NOW initialized (channel %d)", g_espnow_channel);
    }
}

static void Print_Mac(void) {
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    ESP_LOGI(TAG, "Device MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/* ================= 主入口 ================= */
void app_main(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "   IOT Slave Device Starting...");
    ESP_LOGI(TAG, "========================================");
    
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    Print_Mac();
    
    Init_Gpios();
    
    HW_Servo_Init();
    HW_Servo_Set_Angle(BOX_SERVO_ANGLE_CLOSE);
    
    ESP_LOGI(TAG, "Connecting to WiFi...");
    App_WiFi_System_Init();
    
    ESP_LOGI(TAG, "Initializing ESP-NOW...");
    EspNow_Init();
    
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "   IOT Slave Ready!");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "LED Configuration:");
    for (int i = 0; i < MAX_GPIOS; i++) {
        ESP_LOGI(TAG, "  [%d] %s -> GPIO%d", i, g_gpios[i].name, g_gpios[i].pin);
    }
    ESP_LOGI(TAG, "Servo: GPIO %d (initial angle: 90)", SERVO_GPIO);

#ifdef CONFIG_DEBUG_BTN_ENABLED
    Debug_Btn_Init();
    xTaskCreate(Debug_Btn_Task, "debug_btn", 2048, NULL, 5, NULL);
    ESP_LOGI(TAG, "Debug button task started on GPIO %d", DEBUG_BTN_GPIO);
#endif

    ESP_LOGI(TAG, "Waiting for master commands...");
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS));
        
        if (g_master_known) {
            if (!esp_now_is_peer_exist(g_master_mac)) {
                esp_now_peer_info_t peer = {0};
                memcpy(peer.peer_addr, g_master_mac, 6);
                peer.channel = g_espnow_channel;
                peer.ifidx = WIFI_IF_STA;
                peer.encrypt = false;
                esp_now_add_peer(&peer);
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
    }
}
