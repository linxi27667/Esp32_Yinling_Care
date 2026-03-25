#include "yolo_mcp_tool.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "YOLO_MCP";

static SemaphoreHandle_t g_yolo_mutex = NULL;

bool Yolo_Mcp_Init(void) {
    ESP_LOGI(TAG, "Initializing YOLO MCP tool...");
    
    g_yolo_mutex = xSemaphoreCreateMutex();
    if (g_yolo_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return false;
    }
    
    if (!App_Yolo_Init()) {
        ESP_LOGW(TAG, "App_Yolo_Init failed, YOLO will run in stub mode");
    }
    
    ESP_LOGI(TAG, "YOLO MCP tool initialized");
    ESP_LOGI(TAG, "Note: Face detection requires ESP-DL library");
    ESP_LOGI(TAG, "Add to idf_component.yml: espressif/esp-dl: ^1.0.0");
    
    return true;
}

bool Yolo_Mcp_Detect(uint8_t *rgb565_data, int width, int height, yolo_result_t *result) {
    if (rgb565_data == NULL || result == NULL) {
        return false;
    }
    
    if (xSemaphoreTake(g_yolo_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        ESP_LOGW(TAG, "YOLO busy, timeout waiting for mutex");
        return false;
    }
    
    uint32_t start_time = xTaskGetTickCount();
    
    bool success = App_Yolo_Detect(rgb565_data, width, height, result);
    
    uint32_t elapsed = (xTaskGetTickCount() - start_time) * portTICK_PERIOD_MS;
    ESP_LOGI(TAG, "Detection completed in %lu ms, found %d faces", elapsed, result->count);
    
    for (int i = 0; i < result->count; i++) {
        ESP_LOGI(TAG, "Face %d: pos(%d,%d) size(%dx%d) conf=%.2f", 
                 i, result->boxes[i].x, result->boxes[i].y, 
                 result->boxes[i].w, result->boxes[i].h, 
                 result->boxes[i].confidence);
    }
    
    xSemaphoreGive(g_yolo_mutex);
    return success;
}

void Yolo_Mcp_Draw_Boxes(uint8_t *rgb565_data, int width, int height, const yolo_result_t *result) {
    if (rgb565_data == NULL || result == NULL) return;
    
    App_Yolo_Draw_Latest_Boxes(rgb565_data, width, height);
}
