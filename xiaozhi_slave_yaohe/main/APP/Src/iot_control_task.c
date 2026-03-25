/**
 * @file    iot_control_task.c
 * @brief   设备控制任务 (Flag-driven + Enum 架构)
 *
 * 职责：
 * - 维护设备标志位（枚举类型）
 * - 定时刷新 GPIO 输出
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

#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES           LEDC_TIMER_14_BIT
#define LEDC_FREQUENCY          50
#define LEDC_CLK_CFG            LEDC_AUTO_CLK

#define SERVO_MIN_PULSEWIDTH_US 500
#define SERVO_MAX_PULSEWIDTH_US 2500
#define SERVO_MAX_ANGLE         135

/* ================= GPIO 配置 ================= */
#define LED_GPIO_1              GPIO_NUM_2
#define LED_GPIO_2              GPIO_NUM_4
#define LED_GPIO_3              GPIO_NUM_5
#define LED_GPIO_4              GPIO_NUM_48
#define SERVO_GPIO              GPIO_NUM_18
#define REFRESH_INTERVAL_MS     100

/* ================= 药盒配置 ================= */
#define BOX_SERVO_ANGLE_6      -110
#define BOX_SERVO_ANGLE_5      -81
#define BOX_SERVO_ANGLE_4      -45
#define BOX_SERVO_ANGLE_3      -8
#define BOX_SERVO_ANGLE_2       29
#define BOX_SERVO_ANGLE_1       63
#define BOX_SERVO_ANGLE_CLOSE   99

static const int16_t g_box_angle_map[7] = {
    [MED_BOX_CLOSE] = BOX_SERVO_ANGLE_CLOSE,
    [MED_BOX_1]     = BOX_SERVO_ANGLE_1,
    [MED_BOX_2]     = BOX_SERVO_ANGLE_2,
    [MED_BOX_3]     = BOX_SERVO_ANGLE_3,
    [MED_BOX_4]     = BOX_SERVO_ANGLE_4,
    [MED_BOX_5]     = BOX_SERVO_ANGLE_5,
    [MED_BOX_6]     = BOX_SERVO_ANGLE_6
};

/* ================= 内部状态 ================= */
static gpio_num_t g_led_gpios[MAX_GPIOS] = {
    LED_GPIO_1,
    LED_GPIO_2,
    LED_GPIO_3,
    LED_GPIO_4
};

static const char* g_led_names[MAX_GPIOS] = {
    "led1", "led2", "led3", "led4"
};

static int16_t g_servo_angle = BOX_SERVO_ANGLE_CLOSE;

volatile device_flags_enum_t g_device_flags = {
    .light_state = LIGHT_OFF,
    .box_state = MED_BOX_CLOSE
};

/* ================= 调试按钮（DEBUG_MODE） ================= */
#define DEBUG_MODE
#define DEBUG_BTN_GPIO    GPIO_NUM_1

#ifdef DEBUG_MODE
static med_box_state_enum_t g_debug_box_index = MED_BOX_CLOSE;
static uint8_t g_debug_btn_last_state = 1;

static void Debug_Btn_Init(void);
static void HW_Servo_Set_Angle(int16_t angle);

static void Debug_Btn_Task(void* arg) {
    while (1) {
        uint8_t btn_state = gpio_get_level(DEBUG_BTN_GPIO);
        if (btn_state == 0 && g_debug_btn_last_state == 1) {
            g_debug_box_index = (g_debug_box_index + 1) % 7;
            g_device_flags.box_state = g_debug_box_index;
            HW_Servo_Set_Angle(g_box_angle_map[g_debug_box_index]);
            vTaskDelay(pdMS_TO_TICKS(300));
        }
        g_debug_btn_last_state = btn_state;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void Debug_Btn_Init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << DEBUG_BTN_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    ESP_LOGI(TAG, "Debug button initialized on GPIO %d (pull-up)", DEBUG_BTN_GPIO);
}
#endif

/* ================= 硬件底层函数 ================= */
static void HW_Gpio_Init(gpio_num_t pin) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(pin, 0);
}

static void HW_Gpio_Write(gpio_num_t pin, uint8_t level) {
    gpio_set_level(pin, level);
}

static void HW_Servo_Init(void) {
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_MODE,
        .duty_resolution = LEDC_DUTY_RES,
        .timer_num = LEDC_TIMER,
        .freq_hz = LEDC_FREQUENCY,
        .clk_cfg = LEDC_CLK_CFG
    };
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t channel_conf = {
        .gpio_num = SERVO_GPIO,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&channel_conf);

    ESP_LOGI(TAG, "Servo initialized on GPIO %d", SERVO_GPIO);
}

static void HW_Servo_Set_Angle(int16_t angle) {
    if (angle > 135) angle = 135;
    if (angle < -135) angle = -135;

    float pulse_width = SERVO_MIN_PULSEWIDTH_US +
        (float)(angle + 135) * (SERVO_MAX_PULSEWIDTH_US - SERVO_MIN_PULSEWIDTH_US) / (SERVO_MAX_ANGLE * 2);

    uint32_t duty = (uint32_t)((pulse_width * 16384) / 20000);

    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0);

    if (g_servo_angle != angle) {
        g_servo_angle = angle;
        ESP_LOGI(TAG, "Servo: %d deg", angle);
    }
}

/* ================= 设备刷新任务 ================= */
static void Device_Refresh_Task(void* arg) {
    ESP_LOGI(TAG, "Device refresh task started");

    for (int i = 0; i < MAX_GPIOS; i++) {
        HW_Gpio_Init(g_led_gpios[i]);
        ESP_LOGI(TAG, "GPIO[%d] %s -> GPIO%d", i, g_led_names[i], g_led_gpios[i]);
    }

    HW_Servo_Init();
    HW_Servo_Set_Angle(BOX_SERVO_ANGLE_CLOSE);

    while (1) {
        if (g_device_flags.light_state == LIGHT_ON) {
            HW_Gpio_Write(LED_GPIO_1, 1);
        } else {
            HW_Gpio_Write(LED_GPIO_1, 0);
        }

        HW_Servo_Set_Angle(g_box_angle_map[g_device_flags.box_state]);

        vTaskDelay(pdMS_TO_TICKS(REFRESH_INTERVAL_MS));
    }
}

/* ================= 初始化 ================= */
void Iot_Control_Task_Init(void) {
#ifdef DEBUG_MODE
    Debug_Btn_Init();
    xTaskCreatePinnedToCore(Debug_Btn_Task, "debug_btn", 4096, NULL, 5, NULL, 0);
    ESP_LOGI(TAG, "Debug button task started on GPIO %d", DEBUG_BTN_GPIO);
#endif

    BaseType_t ret = xTaskCreate(
        Device_Refresh_Task,
        "iot_ctrl",
        3072,
        NULL,
        3,
        NULL
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Device_Refresh_Task");
    } else {
        ESP_LOGI(TAG, "Device control task started (refresh: %d ms)", REFRESH_INTERVAL_MS);
    }
}
