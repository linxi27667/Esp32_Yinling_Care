#ifndef __BSP_MPU6050_H__
#define __BSP_MPU6050_H__

#include <stdint.h>
#include <driver/i2c_master.h>
#include <driver/gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_USE_SW_I2C_FOR_MPU6050
typedef esp_err_t (*mpu_i2c_read_t)(uint8_t addr, uint8_t reg, uint8_t* data, size_t len);
typedef esp_err_t (*mpu_i2c_write_t)(uint8_t addr, uint8_t reg, uint8_t data);
#endif

#define MPU_SMPLRT_DIV_REG      0x19
#define MPU_CONFIG_REG          0x1A
#define MPU_GYRO_CONFIG_REG     0x1B
#define MPU_ACCEL_CONFIG_REG    0x1C
#define MPU_ACCEL_XOUT_H_REG    0x3B
#define MPU_PWR_MGMT_1_REG      0x6B
#define MPU_WHO_AM_I_REG        0x75

#define MPU6050_ADDR_LOW        0x68
#define MPU6050_ADDR_HIGH       0x69

typedef struct mpu_dev {
#ifdef CONFIG_USE_SW_I2C_FOR_MPU6050
    void* i2c_dev;
    mpu_i2c_read_t read;
    mpu_i2c_write_t write;
#else
    i2c_master_dev_handle_t i2c_dev;
#endif
    uint8_t dev_addr;

    float accel_x, accel_y, accel_z;
    float gyro_x, gyro_y, gyro_z;
    float temp;

    void (*Delay_ms)(uint32_t ms);
} mpu_t;

void MPU6050_Set_I2C_Mutex(void* mutex);
uint8_t MPU6050_Init_Device(mpu_t *mpu);
void MPU6050_Read_Data(mpu_t *mpu);

#ifdef __cplusplus
}
#endif

#endif
