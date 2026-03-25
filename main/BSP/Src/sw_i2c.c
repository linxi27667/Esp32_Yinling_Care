#include "sw_i2c.h"
#include <esp_log.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_timer.h>

static const char* TAG = "SW_I2C";

static void delay_us(uint32_t us) {
    esp_rom_delay_us(us);
}

static void sda_set_mode(sw_i2c_handle_t* handle, bool output) {
    gpio_set_direction(handle->sda_gpio, output ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT);
}

static void sda_write(sw_i2c_handle_t* handle, uint8_t val) {
    gpio_set_level(handle->sda_gpio, val ? 1 : 0);
}

static uint8_t sda_read(sw_i2c_handle_t* handle) {
    return gpio_get_level(handle->sda_gpio);
}

static void scl_write(sw_i2c_handle_t* handle, uint8_t val) {
    gpio_set_level(handle->scl_gpio, val ? 1 : 0);
}

static void i2c_start(sw_i2c_handle_t* handle) {
    sda_set_mode(handle, true);
    sda_write(handle, 1);
    scl_write(handle, 1);
    delay_us(handle->delay_us);
    sda_write(handle, 0);
    delay_us(handle->delay_us);
    scl_write(handle, 0);
    delay_us(handle->delay_us);
}

static void i2c_stop(sw_i2c_handle_t* handle) {
    sda_set_mode(handle, true);
    sda_write(handle, 0);
    scl_write(handle, 1);
    delay_us(handle->delay_us);
    sda_write(handle, 1);
    delay_us(handle->delay_us);
}

static esp_err_t i2c_write_byte(sw_i2c_handle_t* handle, uint8_t data) {
    uint8_t ack;
    sda_set_mode(handle, true);
    for (int i = 7; i >= 0; i--) {
        sda_write(handle, (data >> i) & 1);
        delay_us(handle->delay_us);
        scl_write(handle, 1);
        delay_us(handle->delay_us);
        scl_write(handle, 0);
        delay_us(handle->delay_us);
    }
    sda_set_mode(handle, false);
    delay_us(handle->delay_us);
    scl_write(handle, 1);
    delay_us(handle->delay_us);
    ack = sda_read(handle);
    scl_write(handle, 0);
    delay_us(handle->delay_us);
    return ack ? ESP_FAIL : ESP_OK;
}

static uint8_t i2c_read_byte(sw_i2c_handle_t* handle, bool send_ack) {
    uint8_t data = 0;
    sda_set_mode(handle, false);
    for (int i = 7; i >= 0; i--) {
        scl_write(handle, 1);
        delay_us(handle->delay_us);
        if (sda_read(handle)) {
            data |= (1 << i);
        }
        scl_write(handle, 0);
        delay_us(handle->delay_us);
    }
    sda_set_mode(handle, true);
    sda_write(handle, send_ack ? 0 : 1);
    delay_us(handle->delay_us);
    scl_write(handle, 1);
    delay_us(handle->delay_us);
    scl_write(handle, 0);
    delay_us(handle->delay_us);
    return data;
}

esp_err_t sw_i2c_init(sw_i2c_handle_t* handle, const sw_i2c_config_t* config) {
    if (handle == NULL || config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << config->sda_gpio) | (1ULL << config->scl_gpio),
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    handle->sda_gpio = config->sda_gpio;
    handle->scl_gpio = config->scl_gpio;
    handle->delay_us = config->delay_us > 0 ? config->delay_us : 5;
    handle->initialized = true;

    scl_write(handle, 1);
    sda_write(handle, 1);

    ESP_LOGI(TAG, "Software I2C initialized on SDA=%d, SCL=%d, delay=%lu us",
             config->sda_gpio, config->scl_gpio, handle->delay_us);
    return ESP_OK;
}

esp_err_t sw_i2c_deinit(sw_i2c_handle_t* handle) {
    if (handle == NULL || !handle->initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    gpio_reset_pin(handle->sda_gpio);
    gpio_reset_pin(handle->scl_gpio);
    handle->initialized = false;
    return ESP_OK;
}

esp_err_t sw_i2c_write_reg(sw_i2c_handle_t* handle, uint8_t addr, uint8_t reg, uint8_t data) {
    if (handle == NULL || !handle->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    i2c_start(handle);
    esp_err_t ret = i2c_write_byte(handle, addr << 1);
    if (ret != ESP_OK) {
        i2c_stop(handle);
        return ESP_FAIL;
    }
    i2c_write_byte(handle, reg);
    i2c_write_byte(handle, data);
    i2c_stop(handle);
    return ESP_OK;
}

esp_err_t sw_i2c_read_reg(sw_i2c_handle_t* handle, uint8_t addr, uint8_t reg, uint8_t* data) {
    return sw_i2c_read_reg_n(handle, addr, reg, data, 1);
}

esp_err_t sw_i2c_read_reg_n(sw_i2c_handle_t* handle, uint8_t addr, uint8_t reg, uint8_t* data, size_t len) {
    if (handle == NULL || !handle->initialized || data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_start(handle);
    if (i2c_write_byte(handle, addr << 1) != ESP_OK) {
        i2c_stop(handle);
        return ESP_FAIL;
    }
    i2c_write_byte(handle, reg);
    i2c_start(handle);
    if (i2c_write_byte(handle, (addr << 1) | 1) != ESP_OK) {
        i2c_stop(handle);
        return ESP_FAIL;
    }
    for (size_t i = 0; i < len; i++) {
        bool ack = (i < len - 1);
        data[i] = i2c_read_byte(handle, ack);
    }
    i2c_stop(handle);
    return ESP_OK;
}
