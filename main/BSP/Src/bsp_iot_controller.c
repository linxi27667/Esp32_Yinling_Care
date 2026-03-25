#include "bsp_iot_controller.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "esp_timer.h"

static const char* TAG = "BSP_IOT";

void IOT_Controller_Init(iot_controller_t* dev) {
    if (dev == NULL) return;
    
    dev->local_gpio_count = 0;
    dev->remote_device_count = 0;
    
    memset(dev->local_gpios, 0, sizeof(dev->local_gpios));
    memset(dev->remote_devices, 0, sizeof(dev->remote_devices));
    memset(dev->local_gpio_states, 0, sizeof(dev->local_gpio_states));
    
    if (dev->Log_Info) {
        dev->Log_Info(TAG, "IOT Controller initialized");
    }
}

int IOT_Add_Local_GPIO(iot_controller_t* dev, void* port, uint16_t pin, const char* name, iot_gpio_mode_t mode, uint8_t default_level) {
    if (dev == NULL || dev->local_gpio_count >= IOT_MAX_LOCAL_GPIOS) {
        return -1;
    }
    
    int index = dev->local_gpio_count;
    
    dev->local_gpios[index].port = port;
    dev->local_gpios[index].pin = pin;
    dev->local_gpios[index].mode = mode;
    dev->local_gpios[index].default_level = default_level;
    
    if (name != NULL) {
        strncpy(dev->local_gpios[index].name, name, sizeof(dev->local_gpios[index].name) - 1);
        dev->local_gpios[index].name[sizeof(dev->local_gpios[index].name) - 1] = '\0';
    } else {
        snprintf(dev->local_gpios[index].name, sizeof(dev->local_gpios[index].name), "GPIO%d", pin);
    }
    
    if (dev->Gpio_Init) {
        dev->Gpio_Init(port, pin, mode, default_level);
    }
    
    dev->local_gpio_states[index] = default_level;
    dev->local_gpio_count++;
    
    if (dev->Log_Info) {
        dev->Log_Info(TAG, "Added local GPIO: %s (pin=%d, mode=%d)", 
                      dev->local_gpios[index].name, pin, mode);
    }
    
    return index;
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
            dev->Log_Error(TAG, "GPIO %d is not output mode", index);
        }
        return;
    }
    
    if (dev->Gpio_Write) {
        dev->Gpio_Write(dev->local_gpios[index].port, dev->local_gpios[index].pin, level);
    }
    
    dev->local_gpio_states[index] = level;
    
    if (dev->Log_Info) {
        dev->Log_Info(TAG, "Set local GPIO %s = %d", dev->local_gpios[index].name, level);
    }
}

int IOT_Get_Local_GPIO(iot_controller_t* dev, int index) {
    if (dev == NULL || index < 0 || index >= dev->local_gpio_count) {
        return -1;
    }
    
    if (dev->local_gpios[index].mode == IOT_GPIO_MODE_INPUT && dev->Gpio_Read) {
        return dev->Gpio_Read(dev->local_gpios[index].port, dev->local_gpios[index].pin);
    }
    
    return dev->local_gpio_states[index];
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
            dev->Log_Info(TAG, "Remote device %s is offline, trying anyway...", dev->remote_devices[device_index].device_name);
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

int IOT_Get_Remote_GPIO(iot_controller_t* dev, int device_index, int gpio_index) {
    if (dev == NULL || device_index < 0 || device_index >= dev->remote_device_count) {
        return -1;
    }
    
    iot_command_packet_t cmd = {
        .command = IOT_CMD_GET_GPIO,
        .device_id = dev->remote_devices[device_index].device_id,
        .gpio_index = gpio_index,
        .value = 0,
        .reserved = {0}
    };
    
    if (dev->EspNow_Send) {
        dev->EspNow_Send(dev->remote_devices[device_index].mac_addr, 
                         (const uint8_t*)&cmd, sizeof(cmd));
    }
    
    return -1;
}

int IOT_Find_GPIO_By_Name(iot_controller_t* dev, const char* name, int* device_index, int* gpio_index) {
    if (dev == NULL || name == NULL) {
        return -1;
    }
    
    for (int i = 0; i < dev->local_gpio_count; i++) {
        if (strcmp(dev->local_gpios[i].name, name) == 0) {
            *device_index = -1;
            *gpio_index = i;
            return 0;
        }
    }
    
    for (int d = 0; d < dev->remote_device_count; d++) {
        for (int g = 0; g < dev->remote_devices[d].gpio_count; g++) {
            char full_name[64];
            snprintf(full_name, sizeof(full_name), "%s_%s", 
                     dev->remote_devices[d].device_name, 
                     dev->remote_devices[d].gpios[g].name);
            
            if (strcmp(full_name, name) == 0 || strcmp(dev->remote_devices[d].gpios[g].name, name) == 0) {
                *device_index = d;
                *gpio_index = g;
                return 1;
            }
        }
    }
    
    return -1;
}

int IOT_Find_Device_By_Name(iot_controller_t* dev, const char* name) {
    if (dev == NULL || name == NULL) {
        return -1;
    }
    
    for (int i = 0; i < dev->remote_device_count; i++) {
        if (strcmp(dev->remote_devices[i].device_name, name) == 0) {
            return i;
        }
    }
    
    return -1;
}

void IOT_Process_Remote_Response(iot_controller_t* dev, const uint8_t* mac, const uint8_t* data, int len) {
    if (dev == NULL || data == NULL || len < sizeof(iot_command_packet_t)) {
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
    
    if (found_index >= 0) {
        dev->remote_devices[found_index].online = true;
        
        if (resp->command == IOT_CMD_RESPONSE || resp->command == IOT_CMD_ANNOUNCE) {
            if (dev->Log_Info) {
                dev->Log_Info(TAG, "Response from %s: cmd=%d, gpio=%d, value=%d",
                              dev->remote_devices[found_index].device_name, 
                              resp->command, resp->gpio_index, resp->value);
            }
        }
    } else if (resp->command == IOT_CMD_ANNOUNCE) {
        int gpio_count = resp->gpio_index > 0 ? resp->gpio_index : 4;
        uint8_t has_servo = resp->value & 0x01;

        char device_name[IOT_DEVICE_NAME_LEN];
        snprintf(device_name, sizeof(device_name), "智能药盒");

        int new_index = IOT_Add_Remote_Device(dev, device_name, mac, gpio_count);
        if (new_index >= 0) {
            dev->remote_devices[new_index].online = true;
            if (dev->Log_Info) {
                dev->Log_Info(TAG, "New device discovered: %s (MAC: %02X:%02X:%02X:%02X:%02X:%02X, GPIOs: %d, Servo: %s)",
                              device_name, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                              gpio_count, has_servo ? "Yes" : "No");
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

bool IOT_Set_Remote_Servo(iot_controller_t* dev, int device_index, int gpio_index, uint8_t angle) {
    if (dev == NULL || device_index < 0 || device_index >= dev->remote_device_count) {
        return false;
    }
    
    if (gpio_index < 0 || gpio_index >= dev->remote_devices[device_index].gpio_count) {
        return false;
    }
    
    if (!dev->remote_devices[device_index].online) {
        if (dev->Log_Info) {
            dev->Log_Info(TAG, "Remote device %s is offline, trying anyway...", dev->remote_devices[device_index].device_name);
        }
    }

    iot_command_packet_t cmd = {
        .command = IOT_CMD_SET_SERVO,
        .device_id = dev->remote_devices[device_index].device_id,
        .gpio_index = gpio_index,
        .value = angle,
        .reserved = {0}
    };

    if (dev->EspNow_Send) {
        bool result = dev->EspNow_Send(dev->remote_devices[device_index].mac_addr,
                                        (const uint8_t*)&cmd, sizeof(cmd));
        if (dev->Log_Info) {
            dev->Log_Info(TAG, "Send servo to remote %s GPIO%d = %d deg, result=%d",
                          dev->remote_devices[device_index].device_name, gpio_index, angle, result);
        }
        return result;
    }

    return false;
}

bool IOT_Open_Medicine_Box(iot_controller_t* dev, int device_index, uint8_t box_index) {
    if (dev == NULL || device_index < 0 || device_index >= dev->remote_device_count) {
        return false;
    }

    if (box_index > 7) {
        if (dev->Log_Error) {
            dev->Log_Error(TAG, "Invalid medicine box index: %d (valid: 0-7, 0=close)", box_index);
        }
        return false;
    }

    if (!dev->remote_devices[device_index].online) {
        if (dev->Log_Info) {
            dev->Log_Info(TAG, "Remote device %s is offline, trying anyway...", dev->remote_devices[device_index].device_name);
        }
    }

    iot_command_packet_t cmd = {
        .command = IOT_CMD_OPEN_MEDICINE_BOX,
        .device_id = dev->remote_devices[device_index].device_id,
        .gpio_index = 0,
        .value = box_index,
        .reserved = {0}
    };

    if (dev->EspNow_Send) {
        bool result = dev->EspNow_Send(dev->remote_devices[device_index].mac_addr,
                                        (const uint8_t*)&cmd, sizeof(cmd));
        if (dev->Log_Info) {
            dev->Log_Info(TAG, "Open medicine box %d on remote %s, result=%d",
                          box_index, dev->remote_devices[device_index].device_name, result);
        }
        return result;
    }

    return false;
}

void IOT_Update_Heartbeat(iot_controller_t* dev, const uint8_t* mac) {
    if (dev == NULL || mac == NULL) return;
    
    for (int i = 0; i < dev->remote_device_count; i++) {
        if (memcmp(dev->remote_devices[i].mac_addr, mac, 6) == 0) {
            dev->remote_devices[i].last_heartbeat_ms = (uint32_t)(esp_timer_get_time() / 1000);
            dev->remote_devices[i].online = true;
            if (dev->Log_Info) {
                dev->Log_Info(TAG, "Heartbeat updated for %s", dev->remote_devices[i].device_name);
            }
            return;
        }
    }
}

void IOT_Check_Device_Timeout(iot_controller_t* dev, uint32_t timeout_ms) {
    if (dev == NULL) return;
    
    uint32_t current_ms = (uint32_t)(esp_timer_get_time() / 1000);
    
    for (int i = 0; i < dev->remote_device_count; i++) {
        if (dev->remote_devices[i].online) {
            uint32_t elapsed = current_ms - dev->remote_devices[i].last_heartbeat_ms;
            if (elapsed > timeout_ms) {
                dev->remote_devices[i].online = false;
                if (dev->Log_Info) {
                    dev->Log_Info(TAG, "Device %s timed out (last seen %u ms ago)", 
                                  dev->remote_devices[i].device_name, elapsed);
                }
            }
        }
    }
}
