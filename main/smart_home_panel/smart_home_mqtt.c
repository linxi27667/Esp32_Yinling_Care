#include "smart_home_mqtt.h"
#include "smart_home_ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "esp_event.h"
#include "esp_log.h"
#include "mqtt_client.h"

static const char *TAG = "SmartHomeMQTT";
static esp_mqtt_client_handle_t s_client;
static sh_state_t s_state;

static const char *device_topic_id(sh_device_id_t id)
{
    switch (id) {
        case SH_DEV_DOOR: return "door";
        case SH_DEV_HALL_LIGHT: return "hall_light";
        case SH_DEV_LIVING_LIGHT: return "living_light";
        case SH_DEV_BATH_LIGHT: return "bathroom_light";
        case SH_DEV_MASTER_LIGHT: return "master_light";
        case SH_DEV_RAIN_2: return "rain_2";
        case SH_DEV_RACK_2: return "rack_2";
        case SH_DEV_BALCONY_LIGHT: return "balcony_light";
        case SH_DEV_SKYLIGHT: return "skylight";
        case SH_DEV_RACK_3: return "rack_3";
        case SH_DEV_RAIN_3: return "rain_3";
        case SH_DEV_FLAME_3: return "flame_3";
        default: return "unknown";
    }
}

static sh_device_id_t parse_device_id(const char *device)
{
    if (!device) return SH_DEV_COUNT;
    if (!strcmp(device, "door") || !strcmp(device, "front_door")) return SH_DEV_DOOR;
    if (!strcmp(device, "hall_light")) return SH_DEV_HALL_LIGHT;
    if (!strcmp(device, "living_light") || !strcmp(device, "living_room_light")) return SH_DEV_LIVING_LIGHT;
    if (!strcmp(device, "bathroom_light") || !strcmp(device, "toilet_light")) return SH_DEV_BATH_LIGHT;
    if (!strcmp(device, "master_light") || !strcmp(device, "bedroom_light")) return SH_DEV_MASTER_LIGHT;
    if (!strcmp(device, "rain_2") || !strcmp(device, "rain_sensor_2")) return SH_DEV_RAIN_2;
    if (!strcmp(device, "rack_2") || !strcmp(device, "clothes_rack_2")) return SH_DEV_RACK_2;
    if (!strcmp(device, "balcony_light")) return SH_DEV_BALCONY_LIGHT;
    if (!strcmp(device, "skylight")) return SH_DEV_SKYLIGHT;
    if (!strcmp(device, "rack_3") || !strcmp(device, "clothes_rack_3")) return SH_DEV_RACK_3;
    if (!strcmp(device, "rain_3") || !strcmp(device, "rain_sensor_3")) return SH_DEV_RAIN_3;
    if (!strcmp(device, "flame_3") || !strcmp(device, "flame_sensor_3")) return SH_DEV_FLAME_3;
    return SH_DEV_COUNT;
}

static void copy_json_string(cJSON *root, const char *key, char *dst, size_t dst_size)
{
    cJSON *item = cJSON_GetObjectItem(root, key);
    if (cJSON_IsString(item) && item->valuestring) {
        snprintf(dst, dst_size, "%s", item->valuestring);
    }
}

static int json_int(cJSON *root, const char *key, int fallback)
{
    cJSON *item = cJSON_GetObjectItem(root, key);
    if (cJSON_IsNumber(item)) return item->valueint;
    if (cJSON_IsBool(item)) return cJSON_IsTrue(item) ? 1 : 0;
    return fallback;
}

static bool json_bool(cJSON *root, const char *key, bool fallback)
{
    cJSON *item = cJSON_GetObjectItem(root, key);
    if (cJSON_IsBool(item)) return cJSON_IsTrue(item);
    if (cJSON_IsNumber(item)) return item->valueint != 0;
    if (cJSON_IsString(item) && item->valuestring) {
        return !strcmp(item->valuestring, "on") || !strcmp(item->valuestring, "online") ||
               !strcmp(item->valuestring, "true") || !strcmp(item->valuestring, "1") ||
               !strcmp(item->valuestring, "open");
    }
    return fallback;
}

static void set_default_status(sh_device_id_t id, sh_device_state_t *d)
{
    if (d->status[0]) return;
    switch (id) {
        case SH_DEV_RAIN_2:
        case SH_DEV_RAIN_3:
        case SH_DEV_FLAME_3:
            snprintf(d->status, sizeof(d->status), "正常");
            break;
        case SH_DEV_RACK_2:
        case SH_DEV_RACK_3:
            snprintf(d->status, sizeof(d->status), d->percent > 0 ? "晾晒中" : "已收起");
            break;
        default:
            snprintf(d->status, sizeof(d->status), d->power ? "已开启" : "已关闭");
            break;
    }
}

static void apply_device_json(sh_device_id_t id, cJSON *root)
{
    if (id >= SH_DEV_COUNT || !root) return;
    sh_device_state_t *d = &s_state.devices[id];
    d->valid = true;
    d->online = json_bool(root, "online", true);
    d->power = json_bool(root, "power", d->power);
    d->brightness = json_int(root, "brightness", d->brightness);
    d->percent = json_int(root, "percent", json_int(root, "position", d->percent));
    d->value = json_int(root, "value", json_int(root, "rain", json_int(root, "flame", d->value)));
    d->adc = json_int(root, "adc", d->adc);
    copy_json_string(root, "status", d->status, sizeof(d->status));
    copy_json_string(root, "mode", d->mode, sizeof(d->mode));
    copy_json_string(root, "updated", d->updated, sizeof(d->updated));
    set_default_status(id, d);
}

static void apply_controller_json(int floor, cJSON *root)
{
    if (floor < 1 || floor > 3 || !root) return;
    sh_device_state_t *ctrl = &s_state.controllers[floor - 1];
    ctrl->valid = true;
    ctrl->online = json_bool(root, "online", true);
    copy_json_string(root, "updated", ctrl->updated, sizeof(ctrl->updated));
}

static int parse_floor_from_topic(const char *topic)
{
    const char *p = strstr(topic, "/floor/");
    if (!p) return 0;
    p += strlen("/floor/");
    return atoi(p);
}

static void parse_device_from_topic(const char *topic, char *device, size_t size)
{
    const char *keys[] = {"/device/", "/sensor/"};
    for (int i = 0; i < 2; ++i) {
        const char *p = strstr(topic, keys[i]);
        if (!p) continue;
        p += strlen(keys[i]);
        size_t n = strcspn(p, "/");
        if (n >= size) n = size - 1;
        memcpy(device, p, n);
        device[n] = '\0';
        return;
    }
    device[0] = '\0';
}

static void apply_env_json(cJSON *root)
{
    if (!root) return;
    cJSON *temp = cJSON_GetObjectItem(root, "temperature");
    if (cJSON_IsNumber(temp)) {
        s_state.env.temperature = (float)temp->valuedouble;
        s_state.env.valid = true;
    }
    s_state.env.humidity = json_int(root, "humidity", s_state.env.humidity);
    s_state.env.pm25 = json_int(root, "pm25", s_state.env.pm25);
    s_state.env.total_power = json_int(root, "total_power", json_int(root, "power", s_state.env.total_power));
}

static void apply_scenes_json(cJSON *root)
{
    if (!root) return;
    s_state.scenes.home_mode = json_bool(root, "home", s_state.scenes.home_mode);
    s_state.scenes.away_mode = json_bool(root, "away", s_state.scenes.away_mode);
    s_state.scenes.night_mode = json_bool(root, "night", s_state.scenes.night_mode);
}

static void publish_cb(const char *topic, const char *payload, int qos, bool retain)
{
    if (!s_client || !topic || !payload) return;
    esp_mqtt_client_publish(s_client, topic, payload, 0, qos, retain);
}

static void subscribe_topics(void)
{
    esp_mqtt_client_subscribe(s_client, "smart-home/p4/state/#", 1);
    esp_mqtt_client_subscribe(s_client, "smart-home/floor/+/availability", 1);
    esp_mqtt_client_subscribe(s_client, "smart-home/floor/+/device/+", 1);
    esp_mqtt_client_subscribe(s_client, "smart-home/floor/+/sensor/+", 1);
    esp_mqtt_client_subscribe(s_client, "smart-home/env/state", 1);
    esp_mqtt_client_subscribe(s_client, "smart-home/scene/state", 1);
    esp_mqtt_client_subscribe(s_client, "xiaozhi/iot/heartbeat/+", 0);
    esp_mqtt_client_subscribe(s_client, "xiaozhi/iot/sensor/+", 0);
    esp_mqtt_client_subscribe(s_client, "xiaozhi/iot/announce/+", 1);
}

static void parse_payload(const char *topic, const char *payload)
{
    cJSON *root = cJSON_Parse(payload);
    if (!root) {
        if (strstr(topic, "/availability")) {
            int floor = parse_floor_from_topic(topic);
            cJSON *tmp = cJSON_CreateObject();
            cJSON_AddBoolToObject(tmp, "online", !strcmp(payload, "online"));
            apply_controller_json(floor, tmp);
            cJSON_Delete(tmp);
            smart_home_ui_update_full(&s_state);
        }
        return;
    }

    if (strstr(topic, "/env/") || cJSON_GetObjectItem(root, "temperature") || cJSON_GetObjectItem(root, "pm25")) {
        apply_env_json(root);
    }

    if (strstr(topic, "/scene/") || cJSON_GetObjectItem(root, "home") || cJSON_GetObjectItem(root, "night")) {
        apply_scenes_json(root);
    }

    int floor = parse_floor_from_topic(topic);
    if (strstr(topic, "/availability")) {
        apply_controller_json(floor, root);
    }

    char device_name[40];
    parse_device_from_topic(topic, device_name, sizeof(device_name));
    copy_json_string(root, "device", device_name, sizeof(device_name));
    sh_device_id_t id = parse_device_id(device_name);
    if (id < SH_DEV_COUNT) {
        apply_device_json(id, root);
    }

    cJSON *controller = cJSON_GetObjectItem(root, "controller");
    if (cJSON_IsNumber(controller)) {
        apply_controller_json(controller->valueint, root);
    } else if (floor >= 1 && floor <= 3 && strstr(topic, "heartbeat")) {
        apply_controller_json(floor, root);
    }

    cJSON_Delete(root);
    smart_home_ui_update_full(&s_state);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)base;
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected");
            s_state.network.mqtt_connected = true;
            subscribe_topics();
            smart_home_ui_update_full(&s_state);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT disconnected");
            s_state.network.mqtt_connected = false;
            smart_home_ui_update_full(&s_state);
            break;
        case MQTT_EVENT_DATA: {
            char topic[160];
            char payload[512];
            int topic_len = event->topic_len < (int)sizeof(topic) - 1 ? event->topic_len : (int)sizeof(topic) - 1;
            int data_len = event->data_len < (int)sizeof(payload) - 1 ? event->data_len : (int)sizeof(payload) - 1;
            memcpy(topic, event->topic, topic_len);
            topic[topic_len] = '\0';
            memcpy(payload, event->data, data_len);
            payload[data_len] = '\0';
            ESP_LOGI(TAG, "MQTT data %s %s", topic, payload);
            parse_payload(topic, payload);
            break;
        }
        default:
            break;
    }
}

void smart_home_mqtt_start(void)
{
    if (s_client) return;
    memset(&s_state, 0, sizeof(s_state));
    s_state.language = SH_LANG_ZH;
    s_state.screen_brightness = 80;
    s_state.sound_level = 50;
    for (int i = 0; i < SH_DEV_COUNT; ++i) {
        s_state.devices[i].brightness = -1;
        s_state.devices[i].percent = -1;
        s_state.devices[i].adc = -1;
        s_state.devices[i].value = -1;
    }
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = SH_MQTT_URI,
        .session.keepalive = 30,
        .network.reconnect_timeout_ms = 5000,
    };
    s_client = esp_mqtt_client_init(&cfg);
    if (!s_client) {
        ESP_LOGE(TAG, "Failed to init MQTT client");
        return;
    }
    smart_home_ui_set_mqtt_publish(publish_cb);
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(s_client));
}

void smart_home_mqtt_stop(void)
{
    if (!s_client) return;
    esp_mqtt_client_stop(s_client);
    esp_mqtt_client_destroy(s_client);
    s_client = NULL;
}

void smart_home_mqtt_publish_command(sh_device_id_t id, const char *payload)
{
    if (id >= SH_DEV_COUNT || !payload) return;
    char topic[96];
    int floor = 1;
    if (id >= SH_DEV_LIVING_LIGHT && id <= SH_DEV_RACK_2) floor = 2;
    if (id >= SH_DEV_BALCONY_LIGHT) floor = 3;
    snprintf(topic, sizeof(topic), "smart-home/floor/%d/device/%s/set", floor, device_topic_id(id));
    publish_cb(topic, payload, 1, false);
}

void smart_home_mqtt_set_network_connected(bool connected, const char *ssid)
{
    s_state.network.network_connected = connected;
    if (ssid) {
        snprintf(s_state.network.ssid, sizeof(s_state.network.ssid), "%s", ssid);
    } else if (!connected) {
        s_state.network.ssid[0] = '\0';
    }
    smart_home_ui_update_full(&s_state);
}

const sh_state_t *smart_home_mqtt_get_state(void)
{
    return &s_state;
}
