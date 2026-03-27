/**
 * @file    iot_control_task.c
 * @brief   一楼设备控制任务 (Flag-driven + Enum 架构)
 *
 * 职责：
 * - 维护设备标志位（枚举类型）
 * - 定时刷新 GPIO 输出
 *
 * 设备配置：3灯 + 3继电器 + 3舵机
 *
 * 架构原则：
 * - 设备状态全部使用枚举类型（易读、易扩展）
 * - 只读取标志位并刷新 GPIO
 * - 不处理任何 ESP-NOW 逻辑
 * - 标志位由 esp_now_receive.c 更新
 */
#include "iot_control_task.h"

#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "IOT_CTRL";

/* ================= 1. 硬件底层函数 (HW_ 前缀) ================= */
#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES           LEDC_TIMER_14_BIT
#define LEDC_FREQUENCY          50
#define LEDC_CLK_CFG            LEDC_AUTO_CLK

#define SERVO_MIN_PULSEWIDTH_US 500
#define SERVO_MAX_PULSEWIDTH_US 2500
#define SERVO_MAX_ANGLE         180

static void HW_Gpio_Init(gpio_num_t gpio) {
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << gpio),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&cfg);
}

static void HW_Gpio_Write(gpio_num_t gpio, uint8_t level) {
    if (gpio == GPIO_NUM_48) {
        gpio_set_level(gpio, level ? 0 : 1);
    } else {
        gpio_set_level(gpio, level);
    }
}

static void HW_Servo_Init(gpio_num_t gpio) {
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = LEDC_FREQUENCY,
        .clk_cfg = LEDC_CLK_CFG
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t channel = {
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL_0,
        .gpio_num = gpio,
        .timer_sel = LEDC_TIMER,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&channel);

    ESP_LOGI(TAG, "Servo initialized on GPIO %d", gpio);
}

static void HW_Servo_Set_Angle(gpio_num_t gpio, int16_t angle) {
    if (angle > SERVO_MAX_ANGLE) angle = SERVO_MAX_ANGLE;
    if (angle < 0) angle = 0;

    float pulse_width = SERVO_MIN_PULSEWIDTH_US +
        (float)(angle) * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) / (SERVO_MAX_ANGLE);

    uint32_t duty = (uint32_t)((pulse_width * 16384) / 20000);

    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0);
}

/* ================= 2. GPIO 硬件定义 (人工可改) ================= */
#define LIGHT_GPIO_1     GPIO_NUM_2
#define LIGHT_GPIO_2     GPIO_NUM_4
#define LIGHT_GPIO_3     GPIO_NUM_5

#define RELAY_GPIO_1     GPIO_NUM_6
#define RELAY_GPIO_2     GPIO_NUM_7
#define RELAY_GPIO_3     GPIO_NUM_15

#define SERVO_GPIO_1     GPIO_NUM_18
#define SERVO_GPIO_2     GPIO_NUM_19
#define SERVO_GPIO_3     GPIO_NUM_20

#define REFRESH_INTERVAL_MS     100

/* ================= 3. 舵机角度映射表 ================= */
#define SERVO_ANGLE_0     0
#define SERVO_ANGLE_45    45
#define SERVO_ANGLE_90    90
#define SERVO_ANGLE_135   135

static const int16_t g_servo_angle_map[4] = {
    [SERVO_0]   = SERVO_ANGLE_0,
    [SERVO_45]  = SERVO_ANGLE_45,
    [SERVO_90]  = SERVO_ANGLE_90,
    [SERVO_135] = SERVO_ANGLE_135
};

/* ================= 4. 对象实例化 ================= */
static gpio_num_t g_light_gpios[LIGHT_COUNT] = {
    LIGHT_GPIO_1, LIGHT_GPIO_2, LIGHT_GPIO_3
};

static gpio_num_t g_relay_gpios[RELAY_COUNT] = {
    RELAY_GPIO_1, RELAY_GPIO_2, RELAY_GPIO_3
};

static gpio_num_t g_servo_gpios[SERVO_COUNT] = {
    SERVO_GPIO_1, SERVO_GPIO_2, SERVO_GPIO_3
};

volatile device_flags_enum_t g_device_flags = {
    .light = {OFF, OFF, OFF},
    .relay = {OFF, OFF, OFF},
    .servo = {SERVO_0, SERVO_0, SERVO_0}
};

/* ================= 5. 设备刷新任务 ================= */
static void Device_Refresh_Task(void* arg) {
    ESP_LOGI(TAG, "Device refresh task started");

    for (int i = 0; i < LIGHT_COUNT; i++) {
        HW_Gpio_Init(g_light_gpios[i]);
        ESP_LOGI(TAG, "Light[%d] -> GPIO%d", i, g_light_gpios[i]);
    }

    for (int i = 0; i < RELAY_COUNT; i++) {
        HW_Gpio_Init(g_relay_gpios[i]);
        ESP_LOGI(TAG, "Relay[%d] -> GPIO%d", i, g_relay_gpios[i]);
    }

    for (int i = 0; i < SERVO_COUNT; i++) {
        HW_Servo_Init(g_servo_gpios[i]);
        ESP_LOGI(TAG, "Servo[%d] -> GPIO%d", i, g_servo_gpios[i]);
    }

    while (1) {
        for (int i = 0; i < LIGHT_COUNT; i++) {
            HW_Gpio_Write(g_light_gpios[i], g_device_flags.light[i]);
        }

        for (int i = 0; i < RELAY_COUNT; i++) {
            HW_Gpio_Write(g_relay_gpios[i], g_device_flags.relay[i]);
        }

        for (int i = 0; i < SERVO_COUNT; i++) {
            HW_Servo_Set_Angle(g_servo_gpios[i], g_servo_angle_map[g_device_flags.servo[i]]);
        }

        vTaskDelay(pdMS_TO_TICKS(REFRESH_INTERVAL_MS));
    }
}

/* ================= 6. 对外初始化入口 ================= */
void Iot_Control_Task_Init(void) {
    BaseType_t ret = xTaskCreate(
        Device_Refresh_Task,
        "iot_ctrl",
        4096,
        NULL,
        3,
        NULL
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create device refresh task");
    } else {
        ESP_LOGI(TAG, "Device control initialized: %d lights, %d relays, %d servos",
                 LIGHT_COUNT, RELAY_COUNT, SERVO_COUNT);
    }
}
