#include "app_buzzer.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <driver/gpio.h>

static const char *TAG = "APP_BUZZER";

static void HW_Gpio_Init(gpio_num_t pin, uint8_t mode, uint8_t pull) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode = (mode == 1) ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT,
        .pull_up_en = (pull == 1) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = (pull == 2) ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
}

static void HW_Gpio_Write(gpio_num_t pin, uint8_t level) {
    gpio_set_level(pin, level);
}

static void HW_Delay_ms(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

buzzer_t my_buzzer = {
    .gpio_pin = GPIO_NUM_NC,
    .active_level = 1,
    .Gpio_Init = HW_Gpio_Init,
    .Gpio_Write = HW_Gpio_Write,
    .Delay_ms = HW_Delay_ms,
};

void App_Buzzer_Init(gpio_num_t gpio_pin, uint8_t active_level) {
    my_buzzer.gpio_pin = gpio_pin;
    my_buzzer.active_level = active_level;
    
    Buzzer_Init(&my_buzzer);
    
    ESP_LOGI(TAG, "Buzzer initialized on GPIO %d, active level: %d", gpio_pin, active_level);
}

void App_Buzzer_On(void) {
    Buzzer_On(&my_buzzer);
}

void App_Buzzer_Off(void) {
    Buzzer_Off(&my_buzzer);
}

void App_Buzzer_Beep(uint32_t duration_ms) {
    Buzzer_Beep(&my_buzzer, duration_ms);
}

void App_Buzzer_Beep_Times(uint8_t times, uint32_t on_ms, uint32_t off_ms) {
    Buzzer_Beep_Times(&my_buzzer, times, on_ms, off_ms);
}
