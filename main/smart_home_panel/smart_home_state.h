#ifndef SMART_HOME_STATE_H
#define SMART_HOME_STATE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SH_MQTT_URI "mqtt://8.134.167.240"
#define SH_FIRMWARE_VERSION "v1.2.8"

typedef enum {
    SH_LANG_ZH = 0,
    SH_LANG_EN = 1,
} sh_language_t;

typedef enum {
    SH_CTRL_F1 = 0,
    SH_CTRL_F2,
    SH_CTRL_F3,
    SH_CTRL_COUNT,
} sh_controller_id_t;

typedef enum {
    SH_DEV_DOOR = 0,
    SH_DEV_HALL_LIGHT,
    SH_DEV_LIVING_LIGHT,
    SH_DEV_BATH_LIGHT,
    SH_DEV_MASTER_LIGHT,
    SH_DEV_RAIN_2,
    SH_DEV_RACK_2,
    SH_DEV_BALCONY_LIGHT,
    SH_DEV_SKYLIGHT,
    SH_DEV_RACK_3,
    SH_DEV_RAIN_3,
    SH_DEV_FLAME_3,
    SH_DEV_COUNT,
} sh_device_id_t;

typedef struct {
    bool valid;
    bool online;
    bool power;
    int32_t brightness;
    int32_t percent;
    int32_t value;
    int32_t adc;
    char status[32];
    char mode[24];
    char updated[16];
} sh_device_state_t;

typedef struct {
    bool valid;
    float temperature;
    int32_t humidity;
    int32_t pm25;
    int32_t total_power;
} sh_env_state_t;

typedef struct {
    bool home_mode;
    bool away_mode;
    bool night_mode;
} sh_scene_state_t;

typedef struct {
    bool mqtt_connected;
    bool network_connected;
    char ssid[33];
} sh_network_state_t;

typedef struct {
    sh_device_state_t controllers[SH_CTRL_COUNT];
    sh_device_state_t devices[SH_DEV_COUNT];
    sh_env_state_t env;
    sh_scene_state_t scenes;
    sh_network_state_t network;
    sh_language_t language;
    int32_t screen_brightness;
    int32_t sound_level;
} sh_state_t;

typedef void (*sh_mqtt_publish_fn_t)(const char *topic, const char *payload, int qos, bool retain);

#ifdef __cplusplus
}
#endif

#endif
