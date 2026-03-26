#include "iot_controller.h"
#include "bsp_iot_controller.h"

#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char* TAG = "APP_IOT";

static iot_controller_t g_iot_controller;

#define LOCAL_GPIO_1_PIN    GPIO_NUM_48
#define LOCAL_GPIO_2_PIN    GPIO_NUM_46
#define LOCAL_GPIO_3_PIN    GPIO_NUM_14
#define LOCAL_GPIO_4_PIN    GPIO_NUM_38

static void HW_Gpio_Init(void* port, uint16_t pin, iot_gpio_mode_t mode, uint8_t default_level) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode = mode == IOT_GPIO_MODE_OUTPUT ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = mode == IOT_GPIO_MODE_INPUT ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    
    if (mode == IOT_GPIO_MODE_OUTPUT) {
        gpio_set_level((gpio_num_t)pin, default_level);
    }
}

static void HW_Gpio_Write(void* port, uint16_t pin, uint8_t level) {
    gpio_set_level((gpio_num_t)pin, level);
}

static int HW_Gpio_Read(void* port, uint16_t pin) {
    return gpio_get_level((gpio_num_t)pin);
}

static void HW_EspNow_Recv_Callback(const esp_now_recv_info_t* recv_info, const uint8_t* data, int len) {
    if (recv_info == NULL || recv_info->src_addr == NULL) return;
    
    ESP_LOGI(TAG, "ESP-NOW received %d bytes from %02X:%02X:%02X:%02X:%02X:%02X", 
             len, 
             recv_info->src_addr[0], recv_info->src_addr[1], recv_info->src_addr[2],
             recv_info->src_addr[3], recv_info->src_addr[4], recv_info->src_addr[5]);
    
    if (len >= sizeof(iot_command_packet_t)) {
        iot_command_packet_t* cmd = (iot_command_packet_t*)data;
        if (cmd->command == IOT_CMD_HEARTBEAT) {
            IOT_Update_Heartbeat(&g_iot_controller, recv_info->src_addr);
            return;
        }
    }
    
    IOT_Process_Remote_Response(&g_iot_controller, recv_info->src_addr, data, len);
}

static void HW_EspNow_Send_Callback(const esp_now_send_info_t* tx_info, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGI(TAG, "ESP-NOW send success");
    } else {
        ESP_LOGE(TAG, "ESP-NOW send failed");
    }
}

static bool HW_EspNow_Init(void) {
    esp_err_t ret = esp_now_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW init failed: %s", esp_err_to_name(ret));
        return false;
    }
    
    esp_now_register_recv_cb(HW_EspNow_Recv_Callback);
    esp_now_register_send_cb(HW_EspNow_Send_Callback);
    
    wifi_ap_record_t ap_info;
    ret = esp_wifi_sta_get_ap_info(&ap_info);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Connected to AP: %s, Channel: %d", ap_info.ssid, ap_info.primary);
        
        esp_now_peer_info_t peer = {
            .channel = ap_info.primary,
            .ifidx = WIFI_IF_STA,
            .encrypt = false,
        };
        memcpy(peer.peer_addr, (uint8_t[]){0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 6);
        esp_now_add_peer(&peer);
        
        ESP_LOGI(TAG, "ESP-NOW initialized (channel %d)", ap_info.primary);
    } else {
        ESP_LOGE(TAG, "Failed to get AP info, using default channel 1");
        
        esp_now_peer_info_t peer = {
            .channel = 1,
            .ifidx = WIFI_IF_STA,
            .encrypt = false,
        };
        memcpy(peer.peer_addr, (uint8_t[]){0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 6);
        esp_now_add_peer(&peer);
        
        ESP_LOGI(TAG, "ESP-NOW initialized (channel 1)");
    }
    
    return true;
}

static bool HW_EspNow_Send(const uint8_t* mac, const uint8_t* data, uint16_t len) {
    uint8_t current_channel = 1;
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        current_channel = ap_info.primary;
    }
    
    if (mac[0] != 0xFF || mac[1] != 0xFF || mac[2] != 0xFF || 
        mac[3] != 0xFF || mac[4] != 0xFF || mac[5] != 0xFF) {
        
        if (esp_now_is_peer_exist(mac)) {
            esp_now_peer_info_t peer = {0};
            memcpy(peer.peer_addr, mac, 6);
            peer.channel = current_channel;
            peer.ifidx = WIFI_IF_STA;
            peer.encrypt = false;
            esp_now_mod_peer(&peer);
        } else {
            esp_now_peer_info_t peer = {0};
            memcpy(peer.peer_addr, mac, 6);
            peer.channel = current_channel;
            peer.ifidx = WIFI_IF_STA;
            peer.encrypt = false;
            esp_err_t err = esp_now_add_peer(&peer);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to add peer: %s", esp_err_to_name(err));
            }
        }
    }
    
    esp_err_t ret = esp_now_send(mac, data, len);
    return ret == ESP_OK;
}

static void HW_Delay_Ms(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

static void HW_Log_Info(const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    esp_log_writev(ESP_LOG_INFO, tag, fmt, args);
    va_end(args);
}

static void HW_Log_Error(const char* tag, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    esp_log_writev(ESP_LOG_ERROR, tag, fmt, args);
    va_end(args);
}

static iot_controller_t g_iot_controller = {
    .local_gpio_count = 0,
    .remote_device_count = 0,
    .Gpio_Init = HW_Gpio_Init,
    .Gpio_Write = HW_Gpio_Write,
    .Gpio_Read = HW_Gpio_Read,
    .EspNow_Init = HW_EspNow_Init,
    .EspNow_Send = HW_EspNow_Send,
    .Delay_Ms = HW_Delay_Ms,
    .Log_Info = HW_Log_Info,
    .Log_Error = HW_Log_Error,
};

#define DEVICE_TIMEOUT_MS   60000

static void Device_Timeout_Task(void* arg) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        IOT_Check_Device_Timeout(&g_iot_controller, DEVICE_TIMEOUT_MS);
    }
}

void App_IOT_System_Init(void) {
    ESP_LOGI(TAG, "Initializing IOT Controller...");

    IOT_Controller_Init(&g_iot_controller);
    ESP_LOGI(TAG, "IOT Controller initialized - no local GPIOs (all removed)");

    if (g_iot_controller.EspNow_Init) {
        g_iot_controller.EspNow_Init();
    }

    xTaskCreate(Device_Timeout_Task, "iot_timeout", 2048, NULL, 1, NULL);

    ESP_LOGI(TAG, "IOT Controller initialized with %d local GPIOs", g_iot_controller.local_gpio_count);

    vTaskDelay(pdMS_TO_TICKS(1000));
    ESP_LOGI(TAG, "Auto-discovering nearby devices...");
    App_IOT_Discover_Devices();
}

bool App_IOT_Set_GPIO(const char* device_name, const char* gpio_name, uint8_t level) {
    if (device_name == NULL || strcmp(device_name, "local") == 0) {
        for (int i = 0; i < g_iot_controller.local_gpio_count; i++) {
            if (strcmp(g_iot_controller.local_gpios[i].name, gpio_name) == 0) {
                IOT_Set_Local_GPIO(&g_iot_controller, i, level);
                return true;
            }
        }
        ESP_LOGW(TAG, "Local GPIO '%s' not found", gpio_name);
        return false;
    }
    
    int device_index = IOT_Find_Device_By_Name(&g_iot_controller, device_name);
    if (device_index < 0) {
        ESP_LOGW(TAG, "Remote device '%s' not found", device_name);
        return false;
    }
    
    for (int i = 0; i < g_iot_controller.remote_devices[device_index].gpio_count; i++) {
        if (strcmp(g_iot_controller.remote_devices[device_index].gpios[i].name, gpio_name) == 0) {
            return IOT_Set_Remote_GPIO(&g_iot_controller, device_index, i, level);
        }
    }
    
    ESP_LOGW(TAG, "GPIO '%s' not found on device '%s'", gpio_name, device_name);
    return false;
}

int App_IOT_Get_GPIO(const char* device_name, const char* gpio_name) {
    if (device_name == NULL || strcmp(device_name, "local") == 0) {
        for (int i = 0; i < g_iot_controller.local_gpio_count; i++) {
            if (strcmp(g_iot_controller.local_gpios[i].name, gpio_name) == 0) {
                return IOT_Get_Local_GPIO(&g_iot_controller, i);
            }
        }
    }
    return -1;
}

int App_IOT_Get_Device_Count(void) {
    return g_iot_controller.remote_device_count;
}

int App_IOT_Get_Local_GPIO_Count(void) {
    return g_iot_controller.local_gpio_count;
}

const char* App_IOT_Get_Device_Name(int index) {
    if (index < 0 || index >= g_iot_controller.remote_device_count) {
        return NULL;
    }
    return g_iot_controller.remote_devices[index].device_name;
}

const char* App_IOT_Get_Local_GPIO_Name(int index) {
    if (index < 0 || index >= g_iot_controller.local_gpio_count) {
        return NULL;
    }
    return g_iot_controller.local_gpios[index].name;
}

bool App_IOT_Add_Remote_Device(const char* name, const uint8_t* mac, int gpio_count) {
    int index = IOT_Add_Remote_Device(&g_iot_controller, name, mac, gpio_count);
    return index >= 0;
}

void App_IOT_Discover_Devices(void) {
    IOT_Discover_Devices(&g_iot_controller);
}

bool App_IOT_Set_Servo(const char* device_name, const char* gpio_name, uint8_t angle) {
    int device_index = IOT_Find_Device_By_Name(&g_iot_controller, device_name);
    if (device_index < 0) {
        ESP_LOGW(TAG, "Remote device '%s' not found", device_name);
        return false;
    }
    
    for (int i = 0; i < g_iot_controller.remote_devices[device_index].gpio_count; i++) {
        if (strcmp(g_iot_controller.remote_devices[device_index].gpios[i].name, gpio_name) == 0) {
            return IOT_Set_Remote_Servo(&g_iot_controller, device_index, i, angle);
        }
    }
    
    ESP_LOGW(TAG, "GPIO '%s' not found on device '%s'", gpio_name, device_name);
    return false;
}

bool App_IOT_Open_Medicine_Box(const char* device_name, uint8_t box_index) {
    int device_index = IOT_Find_Device_By_Name(&g_iot_controller, device_name);
    if (device_index < 0) {
        ESP_LOGW(TAG, "Remote device '%s' not found", device_name);
        return false;
    }

    return IOT_Open_Medicine_Box(&g_iot_controller, device_index, box_index);
}

bool App_IOT_Set_Light(const char* device_name, uint8_t light_index, bool on) {
    int device_index = IOT_Find_Device_By_Name(&g_iot_controller, device_name);
    if (device_index < 0) {
        ESP_LOGW(TAG, "Remote device '%s' not found", device_name);
        return false;
    }
    return IOT_Set_Light(&g_iot_controller, device_index, light_index, on ? 1 : 0);
}

bool App_IOT_Set_Relay(const char* device_name, uint8_t relay_index, bool on) {
    int device_index = IOT_Find_Device_By_Name(&g_iot_controller, device_name);
    if (device_index < 0) {
        ESP_LOGW(TAG, "Remote device '%s' not found", device_name);
        return false;
    }
    return IOT_Set_Relay(&g_iot_controller, device_index, relay_index, on ? 1 : 0);
}

bool App_IOT_Set_Servo_ByIndex(const char* device_name, uint8_t servo_index, uint8_t angle) {
    int device_index = IOT_Find_Device_By_Name(&g_iot_controller, device_name);
    if (device_index < 0) {
        ESP_LOGW(TAG, "Remote device '%s' not found", device_name);
        return false;
    }
    return IOT_Set_Servo(&g_iot_controller, device_index, servo_index, angle);
}

bool App_IOT_Broadcast_All_Off(void) {
    return IOT_Broadcast_All_Off(&g_iot_controller);
}

bool App_IOT_Broadcast_All_On(void) {
    return IOT_Broadcast_All_On(&g_iot_controller);
}

void App_IOT_Get_All_Status(char* buffer, int buffer_size) {
    if (buffer == NULL || buffer_size <= 0) return;
    
    int offset = 0;
    
    offset += snprintf(buffer + offset, buffer_size - offset, 
                       "{\"local_gpios\":[");
    
    for (int i = 0; i < g_iot_controller.local_gpio_count && offset < buffer_size - 100; i++) {
        if (i > 0) offset += snprintf(buffer + offset, buffer_size - offset, ",");
        offset += snprintf(buffer + offset, buffer_size - offset,
                          "{\"name\":\"%s\",\"value\":%d}",
                          g_iot_controller.local_gpios[i].name,
                          g_iot_controller.local_gpio_states[i]);
    }
    
    offset += snprintf(buffer + offset, buffer_size - offset, 
                       "],\"remote_devices\":[");
    
    for (int d = 0; d < g_iot_controller.remote_device_count && offset < buffer_size - 100; d++) {
        if (d > 0) offset += snprintf(buffer + offset, buffer_size - offset, ",");
        offset += snprintf(buffer + offset, buffer_size - offset,
                          "{\"name\":\"%s\",\"online\":%s,\"gpio_count\":%d}",
                          g_iot_controller.remote_devices[d].device_name,
                          g_iot_controller.remote_devices[d].online ? "true" : "false",
                          g_iot_controller.remote_devices[d].gpio_count);
    }
    
    offset += snprintf(buffer + offset, buffer_size - offset, "]}");
}
