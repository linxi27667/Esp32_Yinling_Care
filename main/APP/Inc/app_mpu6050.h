#ifndef __APP_MPU6050_H__
#define __APP_MPU6050_H__

#include "bsp_mpu6050.h"

#ifdef __cplusplus
extern "C" {
#endif

extern mpu_t my_mpu;

#ifdef CONFIG_USE_SW_I2C_FOR_MPU6050
void App_MPU6050_Init(void);
#else
void App_MPU6050_Init(i2c_master_bus_handle_t i2c_bus);
#endif
void App_MPU6050_Read(void);

#ifdef __cplusplus
}
#endif

#endif
