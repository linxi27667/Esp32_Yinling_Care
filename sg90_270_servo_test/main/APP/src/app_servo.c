/**
 * @file    servo.c
 * @brief   舵机硬件底层绑定代码
 */

#include "app_servo.h"

#include <stdbool.h>
#include <stdint.h>

#include "driver/ledc.h"
#include "esp_err.h"

#define SERVO_PWM_GPIO         18
#define SERVO_LEDC_SPEED_MODE  LEDC_LOW_SPEED_MODE
#define SERVO_LEDC_TIMER       LEDC_TIMER_0
#define SERVO_LEDC_CHANNEL     LEDC_CHANNEL_0
#define SERVO_PWM_FREQ_HZ      50
#define SERVO_DUTY_RES         LEDC_TIMER_14_BIT
#define SERVO_PERIOD_US        20000U
#define SERVO_DUTY_MAX         ((1U << 14) - 1U)

static void HW_Gpio_Config(void) {
}

static void HW_Tim_Config(void) {
}

static int8_t HW_Servo_Init(void *timer, uint32_t channel) {
    if (timer == NULL) {
        return -1;
    }

    ledc_timer_config_t timer_cfg = {
        .speed_mode = SERVO_LEDC_SPEED_MODE,
        .duty_resolution = SERVO_DUTY_RES,
        .timer_num = SERVO_LEDC_TIMER,
        .freq_hz = SERVO_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false,
    };

    if (ledc_timer_config(&timer_cfg) != ESP_OK) {
        return -1;
    }

    ledc_channel_config_t channel_cfg = {
        .gpio_num = (int)(uintptr_t)timer,
        .speed_mode = SERVO_LEDC_SPEED_MODE,
        .channel = (ledc_channel_t)channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = SERVO_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
        .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
        .flags = {
            .output_invert = 0,
        },
    };

    if (ledc_channel_config(&channel_cfg) != ESP_OK) {
        return -1;
    }

    return 0;
}

static void HW_Servo_Set_Pulse(void *timer, uint32_t channel, uint16_t pulse) {
    if (timer == NULL) {
        return;
    }

    uint32_t duty = ((uint32_t)pulse * SERVO_DUTY_MAX) / SERVO_PERIOD_US;
    ledc_set_duty(SERVO_LEDC_SPEED_MODE, (ledc_channel_t)channel, duty);
    ledc_update_duty(SERVO_LEDC_SPEED_MODE, (ledc_channel_t)channel);
}

servo_t My_Servo_1 = {
    .pwm_pin = {(void *)(uintptr_t)SERVO_PWM_GPIO, SERVO_LEDC_CHANNEL},
    .Gpio_Config = HW_Gpio_Config,
    .Tim_Config = HW_Tim_Config,
    .Init = HW_Servo_Init,
    .Set_Pulse = HW_Servo_Set_Pulse
};

void App_Servo_System_Init(void) {
    Servo_Init_Device(&My_Servo_1);
    Servo_Set_Angle(&My_Servo_1, 0.0f);
}

void App_Servo_Set_Angle(float angle) {
    Servo_Set_Angle(&My_Servo_1, angle);
}
