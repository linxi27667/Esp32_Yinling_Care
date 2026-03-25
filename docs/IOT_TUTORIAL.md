# IoT控制使用教程

## 一、功能概述

本IoT控制系统支持两层架构：

```
┌─────────────────────────────────────────────────────────────┐
│                    小智AI主控设备                            │
│  ┌─────────────────────────────────────────────────────┐   │
│  │              本地GPIO控制 (Local)                     │   │
│  │  led1, led2, relay1, relay2                          │   │
│  └─────────────────────────────────────────────────────┘   │
│                           │                                 │
│                    ESP-NOW 无线通信                         │
│                           │                                 │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐             │
│  │ 远程设备1 │    │ 远程设备2 │    │ 远程设备N │             │
│  │ 客厅灯    │    │ 卧室灯    │  │ 厨房电器  │             │
│  └──────────┘    └──────────┘    └──────────┘             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、本地GPIO控制

### 2.1 可用GPIO

| GPIO名称 | 引脚 | 用途建议 |
|----------|------|----------|
| led1 | GPIO3 | LED指示灯 |
| led2 | GPIO21 | LED指示灯 |
| relay1 | GPIO47 | 继电器1 |
| relay2 | GPIO35 | 继电器2 |

### 2.2 语音控制示例

对小智AI说：
- "打开led1"
- "关闭继电器1"
- "把led2打开"
- "查看设备状态"

---

## 三、远程ESP32从机配置

### 3.1 硬件准备

- ESP32开发板 x1（任意型号，推荐ESP32-S3或ESP32）
- 继电器模块或LED
- 杜邦线若干

### 3.2 从机代码

创建一个新的ESP-IDF项目，包含以下文件：

#### 项目结构
```
iot_slave/
├── main/
│   ├── CMakeLists.txt
│   └── iot_slave_main.c
├── CMakeLists.txt
└── sdkconfig.defaults
```

#### main/iot_slave_main.c

```c
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "IOT_SLAVE";

#define DEVICE_NAME "living_room"
#define GPIO_COUNT 4

static const gpio_num_t control_gpios[GPIO_COUNT] = {
    GPIO_NUM_4,   // GPIO0 - led1
    GPIO_NUM_5,   // GPIO1 - led2
    GPIO_NUM_6,   // GPIO2 - relay1
    GPIO_NUM_7    // GPIO3 - relay2
};

static uint8_t gpio_states[GPIO_COUNT] = {0};

typedef struct {
    uint8_t command;
    uint8_t device_id;
    uint8_t gpio_index;
    uint8_t value;
    uint8_t reserved[4];
} iot_command_packet_t;

enum {
    IOT_CMD_SET_GPIO = 0x01,
    IOT_CMD_GET_GPIO = 0x02,
    IOT_CMD_GET_ALL_GPIO = 0x03,
    IOT_CMD_HEARTBEAT = 0x04,
    IOT_CMD_RESPONSE = 0x05,
    IOT_CMD_DISCOVER = 0x06,
    IOT_CMD_ANNOUNCE = 0x07
};

static void init_gpios(void) {
    for (int i = 0; i < GPIO_COUNT; i++) {
        gpio_reset_pin(control_gpios[i]);
        gpio_set_direction(control_gpios[i], GPIO_MODE_OUTPUT);
        gpio_set_level(control_gpios[i], 0);
    }
    ESP_LOGI(TAG, "GPIOs initialized");
}

static void send_response(const uint8_t *dest_mac, uint8_t cmd, uint8_t gpio_idx, uint8_t value) {
    iot_command_packet_t resp = {
        .command = IOT_CMD_RESPONSE,
        .device_id = 0,
        .gpio_index = gpio_idx,
        .value = value,
        .reserved = {0}
    };
    
    if (cmd == IOT_CMD_DISCOVER) {
        resp.command = IOT_CMD_ANNOUNCE;
    }
    
    esp_now_send(dest_mac, (uint8_t*)&resp, sizeof(resp));
}

static void espnow_recv_cb(const uint8_t *mac_addr, const uint8_t *data, int len) {
    if (len < sizeof(iot_command_packet_t)) {
        return;
    }
    
    iot_command_packet_t *cmd = (iot_command_packet_t *)data;
    ESP_LOGI(TAG, "Received command: %d, gpio: %d, value: %d", 
             cmd->command, cmd->gpio_index, cmd->value);
    
    switch (cmd->command) {
        case IOT_CMD_SET_GPIO:
            if (cmd->gpio_index < GPIO_COUNT) {
                gpio_set_level(control_gpios[cmd->gpio_index], cmd->value);
                gpio_states[cmd->gpio_index] = cmd->value;
                ESP_LOGI(TAG, "Set GPIO %d = %d", cmd->gpio_index, cmd->value);
                send_response(mac_addr, cmd->command, cmd->gpio_index, cmd->value);
            }
            break;
            
        case IOT_CMD_GET_GPIO:
            if (cmd->gpio_index < GPIO_COUNT) {
                send_response(mac_addr, cmd->command, cmd->gpio_index, gpio_states[cmd->gpio_index]);
            }
            break;
            
        case IOT_CMD_GET_ALL_GPIO:
            for (int i = 0; i < GPIO_COUNT; i++) {
                send_response(mac_addr, cmd->command, i, gpio_states[i]);
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            break;
            
        case IOT_CMD_HEARTBEAT:
            send_response(mac_addr, cmd->command, 0, 1);
            break;
            
        case IOT_CMD_DISCOVER:
            ESP_LOGI(TAG, "Discover request received, sending announce");
            send_response(mac_addr, cmd->command, GPIO_COUNT, 0);
            break;
    }
}

static void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status) {
    ESP_LOGD(TAG, "Send status: %d", status);
}

static void init_espnow(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
    
    esp_now_peer_info_t peer = {
        .channel = 1,
        .ifidx = WIFI_IF_STA,
        .encrypt = false,
    };
    memcpy(peer.peer_addr, (uint8_t[]){0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, 6);
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));
    
    ESP_LOGI(TAG, "ESP-NOW initialized");
}

static void print_mac_address(void) {
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    ESP_LOGI(TAG, "Device MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ESP_LOGI(TAG, "Device Name: %s", DEVICE_NAME);
    ESP_LOGI(TAG, "GPIO Count: %d", GPIO_COUNT);
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    ESP_LOGI(TAG, "IOT Slave Device Starting...");
    
    init_gpios();
    init_espnow();
    print_mac_address();
    
    ESP_LOGI(TAG, "IOT Slave Device Ready!");
    ESP_LOGI(TAG, "Waiting for commands from master...");
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
```

#### main/CMakeLists.txt
```cmake
idf_component_register(SRCS "iot_slave_main.c"
                       INCLUDE_DIRS ".")
```

#### CMakeLists.txt (项目根目录)
```cmake
cmake_minimum_required(VERSION 3.5)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(iot_slave)
```

#### sdkconfig.defaults
```
CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE=4096
CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192
CONFIG_FREERTOS_HZ=1000
```

---

## 四、配置远程设备

### 4.1 获取从机MAC地址

1. 编译并烧录从机代码到ESP32
2. 打开串口监视器，查看日志输出：
   ```
   I (1234) IOT_SLAVE: Device MAC: AA:BB:CC:DD:EE:FF
   I (1234) IOT_SLAVE: Device Name: living_room
   I (1234) IOT_SLAVE: GPIO Count: 4
   ```

### 4.2 在主控设备添加远程设备

修改 `main/APP/Src/iot_controller.c`，在 `App_IOT_System_Init` 函数末尾添加：

```c
uint8_t living_room_mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
App_IOT_Add_Remote_Device("living_room", living_room_mac, 4);
```

### 4.3 修改从机GPIO配置

在从机代码中修改：

```c
#define DEVICE_NAME "bedroom"  // 修改设备名称

static const gpio_num_t control_gpios[GPIO_COUNT] = {
    GPIO_NUM_4,   // 根据实际连接修改
    GPIO_NUM_5,
    GPIO_NUM_6,
    GPIO_NUM_7
};
```

---

## 五、语音控制命令

### 5.1 本地设备控制

| 语音命令 | 效果 |
|----------|------|
| "打开led1" | 本地GPIO3输出高电平 |
| "关闭led1" | 本地GPIO3输出低电平 |
| "打开继电器1" | 本地GPIO47输出高电平 |
| "查看设备状态" | 返回所有设备状态 |

### 5.2 远程设备控制

| 语音命令 | 效果 |
|----------|------|
| "打开客厅的灯" | 远程设备living_room的GPIO0 |
| "关闭客厅的继电器" | 远程设备living_room的GPIO2 |
| "搜索远程设备" | 发送发现命令 |

---

## 六、硬件连接示例

### 6.1 主控设备（小智AI）

```
ESP32-S3-N8R8
├── GPIO3  ── LED1 (通过限流电阻)
├── GPIO21 ── LED2 (通过限流电阻)
├── GPIO47 ── 继电器模块 IN1
└── GPIO35 ── 继电器模块 IN2
```

### 6.2 远程从机设备

```
ESP32 (任意型号)
├── GPIO4  ── 客厅灯继电器
├── GPIO5  ── 客厅风扇继电器
├── GPIO6  ── 客厅窗帘继电器
└── GPIO7  ── 客厅插座继电器
```

---

## 七、故障排查

### 7.1 远程设备无响应

1. 检查MAC地址是否正确
2. 确认两个设备在同一WiFi信道
3. 检查ESP-NOW是否初始化成功

### 7.2 GPIO控制无效

1. 检查GPIO引脚是否被其他功能占用
2. 确认GPIO配置为输出模式
3. 检查硬件连接

### 7.3 查看日志

```bash
idf.py monitor
```

---

## 八、扩展开发

### 8.1 添加更多GPIO

修改 `iot_controller.c` 中的GPIO定义：

```c
#define LOCAL_GPIO_5_PIN    GPIO_NUM_XX

// 在 App_IOT_System_Init 中添加
IOT_Add_Local_GPIO(&g_iot_controller, NULL, LOCAL_GPIO_5_PIN, "led3", IOT_GPIO_MODE_OUTPUT, 0);
```

### 8.2 添加输入GPIO

```c
IOT_Add_Local_GPIO(&g_iot_controller, NULL, GPIO_NUM_XX, "button1", IOT_GPIO_MODE_INPUT, 0);
```

### 8.3 自定义MCP工具

在 `custom_esp32s3_cam.cc` 的 `InitializeTools` 中添加新工具。

---

*文档创建时间: 2025年*
