#ifndef __APP_BUZZER_H__
#define __APP_BUZZER_H__

#include "bsp_buzzer.h"
#include <driver/gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

extern buzzer_t my_buzzer;

void App_Buzzer_Init(gpio_num_t gpio_pin, uint8_t active_level);
void App_Buzzer_On(void);
void App_Buzzer_Off(void);
void App_Buzzer_Beep(uint32_t duration_ms);
void App_Buzzer_Beep_Times(uint8_t times, uint32_t on_ms, uint32_t off_ms);

#ifdef __cplusplus
}
#endif

#endif
