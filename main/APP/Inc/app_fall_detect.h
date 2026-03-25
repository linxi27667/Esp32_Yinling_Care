#ifndef __APP_FALL_DETECT_H__
#define __APP_FALL_DETECT_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FALL_DETECT_THRESHOLD_G      2.5f
#define FALL_DETECT_FALL_G           3.5f
#define FALL_CONFIRM_COUNT           3
#define FALL_RECOVERY_TIME_MS        5000
#define FALL_BUZZER_BEEP_INTERVAL_MS 500

void App_Fall_Detect_Init(void);
void App_Fall_Detect_Task(void *params);
void App_Fall_Detect_Update(float accel_x, float accel_y, float accel_z);
bool App_Fall_Detect_Is_Falling(void);
void App_Fall_Detect_Reset(void);

#ifdef __cplusplus
}
#endif

#endif
