#include "app_mpu6050.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>

#ifdef CONFIG_USE_SW_I2C_FOR_MPU6050
#include "sw_i2c.h"
static sw_i2c_handle_t g_sw_i2c_handle = {0};
#ifndef CONFIG_MPU6050_SDA_GPIO
#define CONFIG_MPU6050_SDA_GPIO 3
#endif
#ifndef CONFIG_MPU6050_SCL_GPIO
#define CONFIG_MPU6050_SCL_GPIO 46
#endif
#else
#include <driver/i2c_master.h>
#endif

static const char *TAG = "APP_MPU6050";

static void HW_Delay_ms(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

mpu_t my_mpu = {
    .i2c_dev = NULL,
    .dev_addr = MPU6050_ADDR_LOW,
    .accel_x = 0,
    .accel_y = 0,
    .accel_z = 0,
    .gyro_x = 0,
    .gyro_y = 0,
    .gyro_z = 0,
    .temp = 0,
    .Delay_ms = HW_Delay_ms
};

#ifdef CONFIG_USE_SW_I2C_FOR_MPU6050

esp_err_t sw_i2c_read(uint8_t addr, uint8_t reg, uint8_t* data, size_t len) {
    return sw_i2c_read_reg_n(&g_sw_i2c_handle, addr, reg, data, len);
}

esp_err_t sw_i2c_write(uint8_t addr, uint8_t reg, uint8_t data) {
    return sw_i2c_write_reg(&g_sw_i2c_handle, addr, reg, data);
}

void App_MPU6050_Init(void) {
    sw_i2c_config_t config = {
        .sda_gpio = (gpio_num_t)CONFIG_MPU6050_SDA_GPIO,
        .scl_gpio = (gpio_num_t)CONFIG_MPU6050_SCL_GPIO,
        .delay_us = 5,
    };

    esp_err_t ret = sw_i2c_init(&g_sw_i2c_handle, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Software I2C init failed: %s", esp_err_to_name(ret));
        return;
    }

    my_mpu.i2c_dev = (void*)&g_sw_i2c_handle;
    my_mpu.read = sw_i2c_read;
    my_mpu.write = sw_i2c_write;

    ESP_LOGI(TAG, "MPU6050 configured for software I2C on SDA=%d, SCL=%d",
             CONFIG_MPU6050_SDA_GPIO, CONFIG_MPU6050_SCL_GPIO);

    if (MPU6050_Init_Device(&my_mpu) == 0) {
        ESP_LOGI(TAG, "MPU6050 initialized successfully via software I2C");
    } else {
        ESP_LOGE(TAG, "MPU6050 initialization failed");
    }
}

#else

void App_MPU6050_Init(i2c_master_bus_handle_t i2c_bus) {
    if (i2c_bus == NULL) {
        ESP_LOGE(TAG, "I2C bus handle is NULL");
        return;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = my_mpu.dev_addr,
        .scl_speed_hz = 400000,
    };

    esp_err_t ret = i2c_master_bus_add_device(i2c_bus, &dev_cfg, &my_mpu.i2c_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add I2C device: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "MPU6050 I2C device added at address 0x%02X", my_mpu.dev_addr);

    if (MPU6050_Init_Device(&my_mpu) == 0) {
        ESP_LOGI(TAG, "MPU6050 initialized successfully");
    } else {
        ESP_LOGE(TAG, "MPU6050 initialization failed");
    }
}

#endif

void App_MPU6050_Read(void) {
    MPU6050_Read_Data(&my_mpu);
}
