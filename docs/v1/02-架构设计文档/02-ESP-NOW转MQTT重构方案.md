# ESP-NOW → MQTT 重构方案文档

## 一、项目现状总结

### 1.1 涉及设备

| 设备 | 路径 | 功能 |
|------|------|------|
| 主机 (ESP32-S3) | `xiaozhi-esp32-main/main/` | 小智AI对话 + IoT控制 |
| 一楼从机 | `xiaozhi_slave_Firstfloor/` | 3灯 + 3继电器 + 3舵机 |
| 二楼从机 | `xiaozhi_slave_Secondfloor/` | 3灯 + 3继电器 + 3舵机 |
| 三楼从机 | `xiaozhi_slave_Thirdfloor/` | 3灯 + 3继电器 + 3舵机 + 烟雾/雨滴/求助 |
| P4面板 | `lvgl_prj/` | LVGL显示 + MQTT (已有) |

### 1.2 当前ESP-NOW通信架构

```
主机(S3) ──ESP-NOW广播/单播──→ 一楼/二楼/三楼从机
                    ↑
              心跳保活(30s间隔)
```

### 1.3 命令协议（保留不变）

```c
typedef struct {
    uint8_t command;    // 命令码
    uint8_t device_id;  // 设备ID
    uint8_t gpio_index; // GPIO索引
    uint8_t value;      // 值
    uint8_t reserved[4];
} iot_command_packet_t;

// 命令码定义
IOT_CMD_SET_GPIO = 0x01         // 设置GPIO
IOT_CMD_SET_SERVO = 0x10        // 设置舵机
IOT_CMD_SET_RELAY = 0x12        // 设置继电器
IOT_CMD_HEARTBEAT = 0x04        // 心跳
IOT_CMD_DISCOVER = 0x06         // 设备发现
IOT_CMD_BROADCAST_LIGHTS_ON = 0x33   // 广播开灯
IOT_CMD_BROADCAST_LIGHTS_OFF = 0x32  // 广播关灯
IOT_CMD_BROADCAST_ALL_ON = 0x31       // 广播全开
IOT_CMD_BROADCAST_ALL_OFF = 0x30     // 广播全关
```

---

## 二、MQTT协议设计

### 2.1 Topic设计

| Topic | 方向 | 说明 | QoS |
|-------|------|------|-----|
| `xiaozhi/iot/cmd/broadcast` | 主机→从机 | 广播命令（所有从机订阅） | 1 |
| `xiaozhi/iot/cmd/{device_id}` | 主机→从机 | 单播命令（指定从机订阅） | 1 |
| `xiaozhi/iot/resp/{mac}` | 从机→主机 | 从机响应（主机订阅 `xiaozhi/iot/resp/+`） | 0 |
| `xiaozhi/iot/heartbeat/{mac}` | 从机→主机 | 从机心跳（主机订阅 `xiaozhi/iot/heartbeat/+`） | 0 |
| `xiaozhi/iot/announce` | 从机→主机 | 从机上线声明（主机订阅） | 1 |

### 2.2 Payload设计（复用现有结构）

```json
// 命令格式 (与原ESP-NOW一致)
{
    "command": 1,      // 命令码
    "device_id": 1,     // 设备ID
    "gpio_index": 0,   // GPIO索引
    "value": 1          // 值
}

// 心跳格式
{
    "command": 4,
    "mac": "AA:BB:CC:DD:EE:FF"
}

// 声明格式
{
    "command": 8,
    "device_name": "一楼设备",
    "gpio_count": 9
}
```

### 2.3 从机设备ID分配

| 从机 | device_id | 订阅Topic |
|------|-----------|-----------|
| 一楼设备 | 1 | `xiaozhi/iot/cmd/1` + broadcast |
| 二楼设备 | 2 | `xiaozhi/iot/cmd/2` + broadcast |
| 三楼设备 | 3 | `xiaozhi/iot/cmd/3` + broadcast |

---

## 三、重构实施计划

### 阶段一：从机端改造（仅改传输层）

#### 3.1.1 文件变更清单

**一楼从机：**
| 文件 | 操作 | 改动说明 |
|------|------|----------|
| `APP/Src/esp_now_receive.c` | **删除** | 替换为mqtt_receive.c |
| `APP/Src/esp_now_heartbeat.c` | **删除** | 替换为mqtt_heartbeat.c |
| `APP/Inc/esp_now_receive.h` | **删除** | |
| `APP/Inc/esp_now_heartbeat.h` | **删除** | |
| `APP/Src/mqtt_receive.c` | **新增** | MQTT接收+命令解析 |
| `APP/Src/mqtt_heartbeat.c` | **新增** | MQTT心跳 |
| `APP/Inc/mqtt_receive.h` | **新增** | |
| `APP/Inc/mqtt_heartbeat.h` | **新增** | |
| `iot_slave_main.c` | **修改** | 初始化改MQTT |

**二楼/三楼从机：** 同上

#### 3.1.2 mqtt_receive.c 核心代码结构

```c
#include "mqtt_receive.h"
#include "iot_control_task.h"
#include "esp_log.h"
#include "mqtt_client.h"

static const char* TAG = "MQTT_RECV";
static esp_mqtt_client_handle_t s_mqtt_client = NULL;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected");
            // 订阅广播Topic
            esp_mqtt_client_subscribe(s_mqtt_client, "xiaozhi/iot/cmd/broadcast", 1);
            // 订阅本机单播Topic
            esp_mqtt_client_subscribe(s_mqtt_client, "xiaozhi/iot/cmd/" DEVICE_ID, 1);
            // 发布声明
            mqtt_publish_announce();
            break;
        case MQTT_EVENT_DATA:
            // 解析命令（复用Process_Command逻辑）
            process_mqtt_command(event->topic, event->data, event->data_len);
            break;
        // ... 其他case
    }
}

static void process_mqtt_command(const char* topic, const char* data, int len) {
    // 复用现有Process_Command()逻辑
    // 只需修改来源判断（不再判断src_mac）
}

void MQTT_Receive_Init(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://broker.example.com",
        .credentials.client_id = "xiaozhi_slave_1",
    };
    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_mqtt_client);
}
```

#### 3.1.3 关键改动点

1. **命令解析逻辑不变**：`Process_Command()` 完全保留
2. **响应发送改为MQTT Publish**：
   ```c
   // 原: esp_now_send(dest_mac, ...)
   // 改:
   char topic[64];
   snprintf(topic, sizeof(topic), "xiaozhi/iot/resp/%s", get_mac_string());
   esp_mqtt_client_publish(s_mqtt_client, topic, (const char*)&resp, sizeof(resp), 0, 0);
   ```
3. **设备发现机制变更**：从机启动时自动发布announce到 `xiaozhi/iot/announce`

---

### 阶段二：主机端改造

#### 3.2.1 文件变更清单

| 文件 | 操作 | 改动说明 |
|------|------|----------|
| `APP/Src/iot_controller.c` | **修改** | EspNow_Send → MQTT_Publish |
| `BSP/Src/bsp_iot_controller.c` | **修改** | EspNow_Send函数指针替换 |
| `BSP/Inc/bsp_iot_controller.h` | **修改** | 函数指针类型变更 |

#### 3.2.2 核心改动

**bsp_iot_controller.h 函数指针变更：**
```c
// 原:
bool (*EspNow_Send)(const uint8_t* mac, const uint8_t* data, uint16_t len);

// 改为:
bool (*Mqtt_Publish)(const char* topic, const uint8_t* data, uint16_t len);
```

**iot_controller.c 发送逻辑变更：**
```c
// 原: HW_EspNow_Send()
static bool HW_EspNow_Send(const uint8_t* mac, const uint8_t* data, uint16_t len) {
    esp_now_send(mac, data, len);  // 广播或单播
}

// 改为: HW_MQTT_Publish()
static bool HW_MQTT_Publish(const char* topic, const uint8_t* data, uint16_t len) {
    return esp_mqtt_client_publish(s_mqtt_client, topic, (const char*)data, len, 1, 0) >= 0;
}
```

---

## 四、P4面板接入方案

### 4.1 现有资源

- `lvgl_prj/main/BSP/Src/bsp_mqtt.c` - 已有完整MQTT封装
- `lvgl_prj/main/Tasks/Src/task_ui.c` - 已有UI任务

### 4.2 接入步骤

1. **订阅从机状态Topic**：
   ```c
   BSP_MQTT_Subscribe("xiaozhi/iot/heartbeat/+", 0);  // 心跳
   BSP_MQTT_Subscribe("xiaozhi/iot/resp/+", 0);       // 响应
   ```

2. **注册回调更新UI**：
   ```c
   BSP_MQTT_Init(&mqtt_config, mqtt_callback);
   
   static void mqtt_callback(const char* topic, const char* payload, uint32_t len) {
       // 解析payload，更新UI显示的设备状态
       // 根据topic判断设备，更新对应图标
   }
   ```

3. **发送控制命令**（复用主机逻辑）：
   ```c
   BSP_MQTT_Publish("xiaozhi/iot/cmd/broadcast", json_cmd, 1);
   ```

---

## 五、MQTT Broker选择建议

### 5.1 开发测试阶段

使用公共MQTT Broker（免费）：
- `broker.emqx.io` (port: 1883)
- `test.mosquitto.org` (port: 1883)

### 5.2 生产环境建议

| 方案 | 说明 |
|------|------|
| **自建Broker** | 推荐使用 EMQX 或 Mosquitto，支持更多设备 |
| **云服务** | 阿里云IoT、AWS IoT、腾讯云IoT（需付费） |

### 5.3 从机连接参数（需配置）

```c
// 每台从机需要不同的client_id
#define MQTT_CLIENT_ID "xiaozhi_slave_1"  // 一楼: _1, 二楼: _2, 三楼: _3
#define MQTT_BROKER_URL "mqtt://broker.emqx.io"
#define MQTT_BROKER_PORT 1883
```

---

## 六、风险评估与对策

| 风险 | 等级 | 对策 |
|------|------|------|
| MQTT连接不稳定 | 中 | 实现断线重连 + 心跳保活 |
| 命令顺序错乱 | 低 | 从机端命令处理加锁，同一GPIO操作需串行 |
| 多主机并发控制 | 低 | 从机端命令处理加锁，避免状态竞争 |
| 从机RAM不足 | 中 | ESP32-S3 RAM足够，ESP32-C3需优化 |

---

## 七、测试计划

### 7.1 单元测试

1. MQTT连接/断连测试
2. 单播命令测试（一楼开灯）
3. 广播命令测试（全楼关灯）
4. 心跳保活测试
5. 从机掉线重连测试

### 7.2 集成测试

1. 主机S3发送命令 → 从机执行 → 主机收到响应
2. P4面板订阅状态 → 从机状态变化 → P4面板UI更新
3. 多主机并发控制测试

### 7.3 压力测试

1. 连续1000次命令测试
2. 3台从机同时响应测试
3. 网络波动场景测试

---

## 八、实施优先级

```
第一阶段（优先级最高）:
├── 1. 创建通用MQTT传输层代码
├── 2. 改造一楼从机（最小改动验证）
├── 3. 改造主机IoT控制
└── 4. 验证一楼从机控制

第二阶段:
├── 5. 改造二楼从机
├── 6. 改造三楼从机
└── 7. 全流程联调

第三阶段:
└── 8. P4面板MQTT接入 + UI状态同步
```

---

## 九、代码复用设计

为减少代码重复，建议创建**共享MQTT库**：

```
shared/
├── mqtt_protocol.h      # 统一的协议定义
├── mqtt_command.h       # 命令码定义
└── mqtt_utils.c/h       # MAC地址转换等工具函数
```

各设备编译时包含共享库，避免代码复制。

---

*文档版本: v1.0*
*创建日期: 2026-04-19*
*适用范围: ESP32-S3主机 + 一楼/二楼/三楼从机 + P4面板*
