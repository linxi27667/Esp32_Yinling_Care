#include "app_yolo.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "APP_YOLO";

static yolo_result_t g_yolo_result = {0};
static SemaphoreHandle_t g_yolo_mutex = NULL;

#define RGB565_RED 0xF800

static void Draw_Rect_RGB565(uint16_t *pixels, int w, int h, int bx, int by, int bw, int bh, uint16_t color) {
    if (bx < 0) bx = 0;
    if (by < 0) by = 0;
    if (bx + bw > w) bw = w - bx;
    if (by + bh > h) bh = h - by;

    for (int i = 0; i < bw; i++) {
        if (by >= 0 && by < h) pixels[by * w + bx + i] = color;
        if (by + bh - 1 < h)   pixels[(by + bh - 1) * w + bx + i] = color;
    }
    for (int i = 0; i < bh; i++) {
        if (bx >= 0 && bx < w) pixels[(by + i) * w + bx] = color;
        if (bx + bw - 1 < w)   pixels[(by + i) * w + bx + bw - 1] = color;
    }
}

bool App_Yolo_Init(void) {
    ESP_LOGI(TAG, "YOLO module initialized (stub mode - no ESP-DL)");
    
    g_yolo_mutex = xSemaphoreCreateMutex();
    if (g_yolo_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return false;
    }
    
    ESP_LOGI(TAG, "To enable face detection, add ESP-DL library to idf_component.yml:");
    ESP_LOGI(TAG, "  espressif/esp-dl: ^1.0.0");
    
    return true;
}

bool App_Yolo_Trigger_Async(uint8_t *rgb565_data, int width, int height) {
    ESP_LOGW(TAG, "YOLO async detection not available (ESP-DL not installed)");
    return false;
}

void App_Yolo_Draw_Latest_Boxes(uint8_t *rgb565_data, int width, int height) {
    if (!rgb565_data) return;
    
    if (xSemaphoreTake(g_yolo_mutex, 0) == pdTRUE) {
        for (int i = 0; i < g_yolo_result.count; i++) {
            Draw_Rect_RGB565((uint16_t *)rgb565_data, width, height, 
                             g_yolo_result.boxes[i].x, 
                             g_yolo_result.boxes[i].y, 
                             g_yolo_result.boxes[i].w, 
                             g_yolo_result.boxes[i].h, 
                             RGB565_RED);
        }
        xSemaphoreGive(g_yolo_mutex);
    }
}

bool App_Yolo_Detect(uint8_t *rgb565_data, int width, int height, yolo_result_t *result) {
    if (result == NULL) return false;
    
    result->count = 0;
    
    ESP_LOGW(TAG, "YOLO detection not available (ESP-DL not installed)");
    
    return false;
}
