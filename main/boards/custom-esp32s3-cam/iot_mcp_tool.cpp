#include "iot_mcp_tool.h"
#include "iot_controller.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "IOT_MCP";

bool Iot_Mcp_Init(void) {
    ESP_LOGI(TAG, "Initializing IOT MCP tool...");
    App_IOT_System_Init();
    ESP_LOGI(TAG, "IOT MCP tool initialized");
    return true;
}

bool Iot_Mcp_Set_GPIO(const char* device_name, const char* gpio_name, bool value) {
    ESP_LOGI(TAG, "Set GPIO: device=%s, gpio=%s, value=%d", 
             device_name ? device_name : "local", gpio_name, value);
    return App_IOT_Set_GPIO(device_name, gpio_name, value ? 1 : 0);
}

int Iot_Mcp_Get_GPIO(const char* device_name, const char* gpio_name) {
    return App_IOT_Get_GPIO(device_name, gpio_name);
}

void Iot_Mcp_Get_Status_JSON(char* buffer, int buffer_size) {
    App_IOT_Get_All_Status(buffer, buffer_size);
}

int Iot_Mcp_Get_Local_GPIO_Count(void) {
    return App_IOT_Get_Local_GPIO_Count();
}

const char* Iot_Mcp_Get_Local_GPIO_Name(int index) {
    return App_IOT_Get_Local_GPIO_Name(index);
}

int Iot_Mcp_Get_Remote_Device_Count(void) {
    return App_IOT_Get_Device_Count();
}

const char* Iot_Mcp_Get_Remote_Device_Name(int index) {
    return App_IOT_Get_Device_Name(index);
}

bool Iot_Mcp_Add_Remote_Device(const char* name, const uint8_t* mac, int gpio_count) {
    return App_IOT_Add_Remote_Device(name, mac, gpio_count);
}

void Iot_Mcp_Discover(void) {
    ESP_LOGI(TAG, "Starting device discovery...");
    App_IOT_Discover_Devices();
}

int Iot_Mcp_Wait_For_Response(int timeout_ms) {
    int initial_count = App_IOT_Get_Device_Count();
    int elapsed = 0;
    int check_interval = 100;
    
    while (elapsed < timeout_ms) {
        vTaskDelay(pdMS_TO_TICKS(check_interval));
        elapsed += check_interval;
        
        int current_count = App_IOT_Get_Device_Count();
        if (current_count > initial_count) {
            ESP_LOGI(TAG, "Discovered %d new device(s)", current_count - initial_count);
            return current_count - initial_count;
        }
    }
    
    return 0;
}

bool Iot_Mcp_Set_Servo(const char* device_name, uint8_t angle) {
    ESP_LOGI(TAG, "Set Servo: device=%s, angle=%d", 
             device_name ? device_name : "local", angle);
    return App_IOT_Set_Servo(device_name, "servo", angle);
}
