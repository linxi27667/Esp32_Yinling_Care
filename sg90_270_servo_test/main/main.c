#include "app_servo.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BOX_SERVO_ANGLE_CLOSE           135
#define BOX_SERVO_ANGLE_1               100
#define BOX_SERVO_ANGLE_2               66
#define BOX_SERVO_ANGLE_3               32
#define BOX_SERVO_ANGLE_4               -3
#define BOX_SERVO_ANGLE_5               -38
#define BOX_SERVO_ANGLE_6               -70
#define BOX_SERVO_ANGLE_7               -101

void app_main(void)
{
    App_Servo_System_Init();

    float angles[] = {
        BOX_SERVO_ANGLE_CLOSE,
        BOX_SERVO_ANGLE_1,
        BOX_SERVO_ANGLE_2,
        BOX_SERVO_ANGLE_3,
        BOX_SERVO_ANGLE_4,
        BOX_SERVO_ANGLE_5,
        BOX_SERVO_ANGLE_6,
        BOX_SERVO_ANGLE_7
    };
    uint8_t index = 0;
    uint8_t total = sizeof(angles) / sizeof(angles[0]);

    while (1) {
        App_Servo_Set_Angle(angles[index]);
        vTaskDelay(pdMS_TO_TICKS(1000));

        index = (index + 1) % total;
    }
}
