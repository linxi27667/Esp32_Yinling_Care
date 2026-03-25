#include "bsp_buzzer.h"
#include <esp_log.h>

static const char *TAG = "BSP_BUZZER";

void Buzzer_Init(buzzer_t *buz) {
    if (buz == NULL) {
        ESP_LOGE(TAG, "Invalid buzzer handle");
        return;
    }
    
    if (buz->Gpio_Init) {
        buz->Gpio_Init(buz->gpio_pin, 1, 0);
    }
    
    if (buz->Gpio_Write) {
        buz->Gpio_Write(buz->gpio_pin, !buz->active_level);
    }
    
    ESP_LOGI(TAG, "Buzzer initialized on GPIO %d", buz->gpio_pin);
}

void Buzzer_On(buzzer_t *buz) {
    if (buz == NULL || buz->Gpio_Write == NULL) {
        return;
    }
    buz->Gpio_Write(buz->gpio_pin, buz->active_level);
}

void Buzzer_Off(buzzer_t *buz) {
    if (buz == NULL || buz->Gpio_Write == NULL) {
        return;
    }
    buz->Gpio_Write(buz->gpio_pin, !buz->active_level);
}

void Buzzer_Beep(buzzer_t *buz, uint32_t duration_ms) {
    if (buz == NULL) {
        return;
    }
    
    Buzzer_On(buz);
    
    if (buz->Delay_ms) {
        buz->Delay_ms(duration_ms);
    }
    
    Buzzer_Off(buz);
}

void Buzzer_Beep_Times(buzzer_t *buz, uint8_t times, uint32_t on_ms, uint32_t off_ms) {
    if (buz == NULL || times == 0) {
        return;
    }
    
    for (uint8_t i = 0; i < times; i++) {
        Buzzer_Beep(buz, on_ms);
        
        if (buz->Delay_ms && i < times - 1) {
            buz->Delay_ms(off_ms);
        }
    }
}
