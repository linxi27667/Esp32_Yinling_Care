/**
 * @file    bsp_iot_controller.c
 * @brief   BSP IOT 控制器实现
 *
 * 职责：
 * - 管理远程设备列表
 * - 处理 ESP-NOW 通信协议
 * - 提供设备查询和控制接口
 */
#include "bsp_iot_controller.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>
#include <stdio.h>

static const char* TAG = "BSP_IOT";

typedef struct {
    const char* alias;
    const char* real_name;
} device_name_alias_t;

static const device_name_alias_t g_device_aliases[] = {
    {"一楼大门", "一楼设备"},
    {"一楼设备", "一楼设备"},
    {"一楼", "一楼设备"},
    {"大厅灯", "一楼设备"},
    {"二楼设备", "二楼设备"},
    {"二楼", "二楼设备"},
    {"主卧", "二楼设备"},
    {"主卧灯", "二楼设备"},
    {"次卧", "二楼设备"},
    {"次卧灯", "二楼设备"},
    {"客厅", "二楼设备"},
    {"二楼风扇", "二楼设备"},
    {"风扇", "二楼设备"},
    {"三楼", "三楼设备"},
    {"三楼设备", "三楼设备"},
    {"阳台灯", "三楼设备"},
    {NULL, NULL}
};

static const char* IOT_Resolve_Alias(const char* name) {
    if (name == NULL) return NULL;
    for (int i = 0; g_device_aliases[i].alias != NULL; i++) {
        if (strcmp(name, g_device_aliases[i].alias) == 0) {
            return g_device_aliases[i].real_name;
        }
    }
    return name;
}

int IOT_Add_Remote_Device(iot_controller_t* dev, const char* name, const uint8_t* mac, int gpio_count) {
    if (dev == NULL || dev->remote_device_count >= IOT_MAX_REMOTE_DEVICES) {
        return -1;
    }

    int index = dev->remote_device_count;

    if (name != NULL) {
        strncpy(dev->remote_devices[index].device_name, name, IOT_DEVICE_NAME_LEN - 1);
        dev->remote_devices[index].device_name[IOT_DEVICE_NAME_LEN - 1] = '\0';
    }

    dev->remote_devices[index].device_id = index + 1;

    if (mac != NULL) {
        memcpy(dev->remote_devices[index].mac_addr, mac, 6);
    }

    dev->remote_devices[index].gpio_count = gpio_count > IOT_MAX_REMOTE_GPIOS ? IOT_MAX_REMOTE_GPIOS : gpio_count;
    dev->remote_devices[index].online = true;
    dev->remote_devices[index].last_heartbeat_ms = (uint32_t)(esp_timer_get_time() / 1000);

    const char* default_names[4] = {"led1", "led2", "led3", "led4"};
    for (int i = 0; i < dev->remote_devices[index].gpio_count; i++) {
        if (i < 4) {
            strncpy(dev->remote_devices[index].gpios[i].name, default_names[i], 15);
        } else {
            snprintf(dev->remote_devices[index].gpios[i].name, 16, "gpio%d", i);
        }
        dev->remote_devices[index].gpios[i].mode = IOT_GPIO_MODE_OUTPUT;
    }

    dev->remote_device_count++;

    if (dev->Log_Info) {
        dev->Log_Info(TAG, "Added remote device: %s (id=%d, gpios=%d)",
                      dev->remote_devices[index].device_name,
                      dev->remote_devices[index].device_id,
                      dev->remote_devices[index].gpio_count);
    }

    return index;
}

void IOT_Set_Local_GPIO(iot_controller_t* dev, int index, uint8_t level) {
    if (dev == NULL || index < 0 || index >= dev->local_gpio_count) {
        return;
    }

    if (dev->local_gpios[index].mode != IOT_GPIO_MODE_OUTPUT) {
        if (dev->Log_Error) {
            dev->Log_Error(TAG, "GPIO %d is not configured as output", index);
        }
        return;
    }

    if (dev->Gpio_Write) {
        dev->Gpio_Write(dev->local_gpios[index].port, dev->local_gpios[index].pin, level);
        dev->local_gpio_states[index] = level;
    }
}

int IOT_Get_Local_GPIO(iot_controller_t* dev, int index) {
    if (dev == NULL || index < 0 || index >= dev->local_gpio_count) {
        return -1;
    }

    if (dev->Gpio_Read) {
        return dev->Gpio_Read(dev->local_gpios[index].port, dev->local_gpios[index].pin);
    }

    return dev->local_gpio_states[index];
}

int IOT_Find_Device_By_Name(iot_controller_t* dev, const char* name) {
    if (dev == NULL || name == NULL) {
        return -1;
    }

    const char* resolved_name = IOT_Resolve_Alias(name);

    for (int i = 0; i < dev->remote_device_count; i++) {
        if (strcmp(dev->remote_devices[i].device_name, resolved_name) == 0) {
            return i;
        }
    }

    return -1;
}

int IOT_Find_Device_By_Mac(iot_controller_t* dev, const uint8_t* mac) {
    if (dev == NULL || mac == NULL) {
        return -1;
    }

    for (int i = 0; i < dev->remote_device_count; i++) {
        if (memcmp(dev->remote_devices[i].mac_addr, mac, 6) == 0) {
            return i;
        }
    }

    return -1;
}

void IOT_Update_Heartbeat(iot_controller_t* dev, const uint8_t* mac) {
    if (dev == NULL || mac == NULL) {
        return;
    }

    int index = IOT_Find_Device_By_Mac(dev, mac);
    if (index >= 0) {
        dev->remote_devices[index].online = true;
        dev->remote_devices[index].last_heartbeat_ms = (uint32_t)(esp_timer_get_time() / 1000);
    }
}

bool IOT_Set_Remote_GPIO(iot_controller_t* dev, int device_index, int gpio_index, uint8_t level) {
    if (dev == NULL || device_index < 0 || device_index >= dev->remote_device_count) {
        return false;
    }

    if (gpio_index < 0 || gpio_index >= dev->remote_devices[device_index].gpio_count) {
        return false;
    }

    if (!dev->remote_devices[device_index].online) {
        if (dev->Log_Info) {
            dev->Log_Info(TAG, "Remote device %s is offline, trying anyway...",
                          dev->remote_devices[device_index].device_name);
        }
    }

    iot_command_packet_t cmd = {
        .command = IOT_CMD_SET_GPIO,
        .device_id = dev->remote_devices[device_index].device_id,
        .gpio_index = gpio_index,
        .value = level,
        .reserved = {0}
    };

    if (dev->EspNow_Send) {
        bool result = dev->EspNow_Send(dev->remote_devices[device_index].mac_addr,
                                        (const uint8_t*)&cmd, sizeof(cmd));
        if (dev->Log_Info) {
            dev->Log_Info(TAG, "Send to remote %s GPIO%d = %d, result=%d",
                          dev->remote_devices[device_index].device_name, gpio_index, level, result);
        }
        return result;
    }

    return false;
}

bool IOT_Set_Remote_Servo(iot_controller_t* dev, int device_index, int servo_index, uint8_t angle) {
    if (dev == NULL || device_index < 0 || device_index >= dev->remote_device_count) {
        return false;
    }

    if (servo_index < 0 || servo_index >= dev->remote_devices[device_index].gpio_count) {
        return false;
    }

    iot_command_packet_t cmd = {
        .command = IOT_CMD_SET_SERVO,
        .device_id = dev->remote_devices[device_index].device_id,
        .gpio_index = servo_index,
        .value = angle,
        .reserved = {0}
    };

    if (dev->EspNow_Send) {
        return dev->EspNow_Send(dev->remote_devices[device_index].mac_addr,
                                (const uint8_t*)&cmd, sizeof(cmd));
    }

    return false;
}

int IOT_Get_Remote_GPIO(iot_controller_t* dev, int device_index, int gpio_index) {
    if (dev == NULL || device_index < 0 || device_index >= dev->remote_device_count) {
        return -1;
    }

    if (gpio_index < 0 || gpio_index >= dev->remote_devices[device_index].gpio_count) {
        return -1;
    }

    return -1;
}

void IOT_Process_Remote_Response(iot_controller_t* dev, const uint8_t* mac, const uint8_t* data, int len) {
    if (dev == NULL || data == NULL || len < (int)sizeof(iot_command_packet_t)) {
        ESP_LOGW(TAG, "Process_Remote_Response: invalid params, len=%d, sizeof=%d", len, (int)sizeof(iot_command_packet_t));
        return;
    }

    iot_command_packet_t* resp = (iot_command_packet_t*)data;

    int found_index = -1;
    for (int i = 0; i < dev->remote_device_count; i++) {
        if (memcmp(dev->remote_devices[i].mac_addr, mac, 6) == 0) {
            found_index = i;
            break;
        }
    }

    ESP_LOGI(TAG, "Process_Remote_Response: mac=%02X:%02X:%02X:%02X:%02X:%02X, cmd=%d, found=%d, len=%d",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], resp->command, found_index, len);

    if (found_index >= 0) {
        dev->remote_devices[found_index].online = true;

        ESP_LOGI(TAG, "Device already exists [%s], checking for ANNOUNCE_V2...",
                 dev->remote_devices[found_index].device_name);

        if (resp->command == IOT_CMD_ANNOUNCE_V2 && len >= (int)sizeof(iot_announce_v2_packet_t)) {
            iot_announce_v2_packet_t* announce = (iot_announce_v2_packet_t*)data;
            ESP_LOGI(TAG, ">>> ANNOUNCE_V2 received: name='%s', gpio_index=%d, len=%d",
                     announce->device_name, announce->gpio_index, len);
            strncpy(dev->remote_devices[found_index].device_name, announce->device_name, IOT_DEVICE_NAME_LEN - 1);
            dev->remote_devices[found_index].device_name[IOT_DEVICE_NAME_LEN - 1] = '\0';
            dev->remote_devices[found_index].gpio_count = announce->gpio_index > 0 ? announce->gpio_index : 4;
            if (dev->Log_Info) {
                dev->Log_Info(TAG, "Updated device info: %s (MAC: %02X:%02X:%02X:%02X:%02X:%02X, GPIOs: %d)",
                              announce->device_name, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                              dev->remote_devices[found_index].gpio_count);
            }
        } else {
            ESP_LOGW(TAG, ">>> Not ANNOUNCE_V2: cmd=%d, len=%d < sizeof(iot_announce_v2_packet_t)=%d",
                     resp->command, len, (int)sizeof(iot_announce_v2_packet_t));
        }
    } else if (resp->command == IOT_CMD_ANNOUNCE) {
        int gpio_count = resp->gpio_index > 0 ? resp->gpio_index : 4;
        uint8_t has_servo = resp->value & 0x01;

        char device_name[IOT_DEVICE_NAME_LEN];
        snprintf(device_name, sizeof(device_name), "未知设备");

        int new_index = IOT_Add_Remote_Device(dev, device_name, mac, gpio_count);
        if (new_index >= 0) {
            dev->remote_devices[new_index].online = true;
            if (dev->Log_Info) {
                dev->Log_Info(TAG, "New device discovered: %s (MAC: %02X:%02X:%02X:%02X:%02X:%02X, GPIOs: %d, Servo: %s)",
                              device_name, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                              gpio_count, has_servo ? "Yes" : "No");
            }
        }
    } else if (resp->command == IOT_CMD_ANNOUNCE_V2 && len >= (int)sizeof(iot_announce_v2_packet_t)) {
        iot_announce_v2_packet_t* announce = (iot_announce_v2_packet_t*)data;
        int gpio_count = announce->gpio_index > 0 ? announce->gpio_index : 4;

        int new_index = IOT_Add_Remote_Device(dev, announce->device_name, mac, gpio_count);
        if (new_index >= 0) {
            dev->remote_devices[new_index].online = true;
            if (dev->Log_Info) {
                dev->Log_Info(TAG, "New device discovered: %s (MAC: %02X:%02X:%02X:%02X:%02X:%02X, GPIOs: %d)",
                              announce->device_name, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                              gpio_count);
            }
        }
    }
}

void IOT_Send_Heartbeat(iot_controller_t* dev) {
    if (dev == NULL) return;

    iot_command_packet_t cmd = {
        .command = IOT_CMD_HEARTBEAT,
        .device_id = 0,
        .gpio_index = 0,
        .value = 0,
        .reserved = {0}
    };

    if (dev->EspNow_Send) {
        uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        dev->EspNow_Send(broadcast_mac, (const uint8_t*)&cmd, sizeof(cmd));
    }
}

void IOT_Discover_Devices(iot_controller_t* dev) {
    if (dev == NULL) return;

    iot_command_packet_t cmd = {
        .command = IOT_CMD_DISCOVER,
        .device_id = 0,
        .gpio_index = 0,
        .value = 0,
        .reserved = {0}
    };

    if (dev->EspNow_Send) {
        uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        bool result = dev->EspNow_Send(broadcast_mac, (const uint8_t*)&cmd, sizeof(cmd));

        if (dev->Log_Info) {
            dev->Log_Info(TAG, "Sent discover command (result=%d, size=%d)", result, (int)sizeof(cmd));
        }
    } else {
        if (dev->Log_Error) {
            dev->Log_Error(TAG, "EspNow_Send not initialized!");
        }
    }
}

void IOT_Controller_Init(iot_controller_t* dev) {
    if (dev == NULL) return;
    dev->remote_device_count = 0;
    dev->local_gpio_count = 0;
}

bool IOT_Set_Light(iot_controller_t* dev, int device_index, uint8_t light_index, uint8_t on) {
    if (dev == NULL || device_index < 0 || device_index >= dev->remote_device_count) {
        return false;
    }
    int gpio_index = light_index;
    if (gpio_index < 0 || gpio_index >= dev->remote_devices[device_index].gpio_count) {
        return false;
    }
    return IOT_Set_Remote_GPIO(dev, device_index, gpio_index, on);
}

bool IOT_Set_Relay(iot_controller_t* dev, int device_index, uint8_t relay_index, uint8_t on) {
    if (dev == NULL || device_index < 0 || device_index >= dev->remote_device_count) {
        return false;
    }
    int gpio_index = 3 + relay_index;
    if (gpio_index < 0 || gpio_index >= dev->remote_devices[device_index].gpio_count) {
        return false;
    }
    return IOT_Set_Remote_GPIO(dev, device_index, gpio_index, on);
}

bool IOT_Broadcast_All_Off(iot_controller_t* dev) {
    if (dev == NULL) return false;

    iot_command_packet_t cmd = {
        .command = IOT_CMD_BROADCAST_ALL_OFF,
        .device_id = 0,
        .gpio_index = 0,
        .value = 0,
        .reserved = {0}
    };

    if (dev->EspNow_Send) {
        uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        return dev->EspNow_Send(broadcast_mac, (const uint8_t*)&cmd, sizeof(cmd));
    }
    return false;
}

bool IOT_Broadcast_All_On(iot_controller_t* dev) {
    if (dev == NULL) return false;

    iot_command_packet_t cmd = {
        .command = IOT_CMD_BROADCAST_ALL_ON,
        .device_id = 0,
        .gpio_index = 0,
        .value = 0,
        .reserved = {0}
    };

    if (dev->EspNow_Send) {
        uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        return dev->EspNow_Send(broadcast_mac, (const uint8_t*)&cmd, sizeof(cmd));
    }
    return false;
}

bool IOT_Broadcast_Lights_Off(iot_controller_t* dev) {
    if (dev == NULL) return false;

    iot_command_packet_t cmd = {
        .command = IOT_CMD_BROADCAST_LIGHTS_OFF,
        .device_id = 0,
        .gpio_index = 0,
        .value = 0,
        .reserved = {0}
    };

    if (dev->EspNow_Send) {
        uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        return dev->EspNow_Send(broadcast_mac, (const uint8_t*)&cmd, sizeof(cmd));
    }
    return false;
}

bool IOT_Broadcast_Lights_On(iot_controller_t* dev) {
    if (dev == NULL) return false;

    iot_command_packet_t cmd = {
        .command = IOT_CMD_BROADCAST_LIGHTS_ON,
        .device_id = 0,
        .gpio_index = 0,
        .value = 0,
        .reserved = {0}
    };

    if (dev->EspNow_Send) {
        uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        return dev->EspNow_Send(broadcast_mac, (const uint8_t*)&cmd, sizeof(cmd));
    }
    return false;
}

void IOT_Check_Device_Timeout(iot_controller_t* dev, uint32_t timeout_ms) {
    if (dev == NULL) return;

    uint32_t current_time_ms = (uint32_t)(esp_timer_get_time() / 1000);

    for (int i = 0; i < dev->remote_device_count; i++) {
        if (!dev->remote_devices[i].online) continue;

        if (current_time_ms - dev->remote_devices[i].last_heartbeat_ms > timeout_ms) {
            dev->remote_devices[i].online = false;
            if (dev->Log_Info) {
                dev->Log_Info(TAG, "Device %s timed out", dev->remote_devices[i].device_name);
            }
        }
    }
}

bool IOT_Open_Medicine_Box(iot_controller_t* dev, int device_index, uint8_t box_index) {
    if (dev == NULL || device_index < 0 || device_index >= dev->remote_device_count) {
        return false;
    }

    if (box_index > 6) {
        return false;
    }

    iot_command_packet_t cmd = {
        .command = IOT_CMD_OPEN_MEDICINE_BOX,
        .device_id = dev->remote_devices[device_index].device_id,
        .gpio_index = box_index,
        .value = box_index,
        .reserved = {0}
    };

    if (dev->EspNow_Send) {
        return dev->EspNow_Send(dev->remote_devices[device_index].mac_addr,
                                (const uint8_t*)&cmd, sizeof(cmd));
    }

    return false;
}

bool IOT_Set_Servo(iot_controller_t* dev, int device_index, uint8_t servo_index, uint8_t angle) {
    if (dev == NULL || device_index < 0 || device_index >= dev->remote_device_count) {
        return false;
    }
    int gpio_index = 6 + servo_index;
    if (gpio_index < 0 || gpio_index >= dev->remote_devices[device_index].gpio_count) {
        return false;
    }
    return IOT_Set_Remote_Servo(dev, device_index, gpio_index, angle);
}