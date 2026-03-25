#ifndef __BSP_BUZZER_H__
#define __BSP_BUZZER_H__

#include <stdint.h>
#include <driver/gpio.h>

typedef struct buzzer_dev {
    gpio_num_t gpio_pin;
    uint8_t active_level;
    
    void (*Gpio_Init)(gpio_num_t pin, uint8_t mode, uint8_t pull);
    void (*Gpio_Write)(gpio_num_t pin, uint8_t level);
    void (*Delay_ms)(uint32_t ms);
} buzzer_t;

void Buzzer_Init(buzzer_t *buz);
void Buzzer_On(buzzer_t *buz);
void Buzzer_Off(buzzer_t *buz);
void Buzzer_Beep(buzzer_t *buz, uint32_t duration_ms);
void Buzzer_Beep_Times(buzzer_t *buz, uint8_t times, uint32_t on_ms, uint32_t off_ms);

#endif
