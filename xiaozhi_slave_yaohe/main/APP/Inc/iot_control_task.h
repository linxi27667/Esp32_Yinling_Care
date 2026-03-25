/**
 * @file    iot_control_task.h
 * @brief   设备控制任务 (Flag-driven + Enum 架构)
 *
 * 职责：
 * - 维护设备标志位（枚举类型）
 * - 定时刷新 GPIO 输出
 *
 * 架构原则：
 * - 设备状态全部使用枚举类型（易读、易扩展）
 * - 枚举定义在此处，供 esp_now_receive.c 引用
 */
#ifndef IOT_CONTROL_TASK_H
#define IOT_CONTROL_TASK_H

#include <stdint.h>

#define MAX_GPIOS  4

/* ================= 设备状态枚举 ================= */
typedef enum {
    LIGHT_OFF = 0,
    LIGHT_ON = 1
} light_state_enum_t;

typedef enum {
    MED_BOX_CLOSE = 0,
    MED_BOX_1 = 1,
    MED_BOX_2 = 2,
    MED_BOX_3 = 3,
    MED_BOX_4 = 4,
    MED_BOX_5 = 5,
    MED_BOX_6 = 6
} med_box_state_enum_t;

/* ================= 设备标志位结构体 ================= */
typedef struct {
    light_state_enum_t light_state;
    med_box_state_enum_t box_state;
} device_flags_enum_t;

extern volatile device_flags_enum_t g_device_flags;

void Iot_Control_Task_Init(void);

#endif
