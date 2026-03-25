#ifndef __SW_I2C_H__
#define __SW_I2C_H__

#include <driver/gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gpio_num_t sda_gpio;
    gpio_num_t scl_gpio;
    uint32_t delay_us;
} sw_i2c_config_t;

typedef struct {
    gpio_num_t sda_gpio;
    gpio_num_t scl_gpio;
    uint32_t delay_us;
    bool initialized;
} sw_i2c_handle_t;

esp_err_t sw_i2c_init(sw_i2c_handle_t* handle, const sw_i2c_config_t* config);
esp_err_t sw_i2c_deinit(sw_i2c_handle_t* handle);
esp_err_t sw_i2c_write_reg(sw_i2c_handle_t* handle, uint8_t addr, uint8_t reg, uint8_t data);
esp_err_t sw_i2c_read_reg(sw_i2c_handle_t* handle, uint8_t addr, uint8_t reg, uint8_t* data);
esp_err_t sw_i2c_read_reg_n(sw_i2c_handle_t* handle, uint8_t addr, uint8_t reg, uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif

#endif
