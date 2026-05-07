# MQTT控制链验证与Broker配置计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 验证主机是否通过MQTT Broker正确控制所有从机设备，并配置稳定的MQTT Broker

**Architecture:** 主机(xiaozhi_master)和3个从机(xiaozhi_slave_1/2/3)通过MQTT Broker通信，使用Topic路由命令和响应

**Tech Stack:** ESP-IDF MQTT Client, EMQX Broker, FreeRTOS

---

## 当前状态分析

### 已完成的MQTT重构
- ✅ 主机 `iot_controller.c` - MQTT初始化、事件处理、命令发送
- ✅ 从机 `mqtt_receive.c` - MQTT连接、命令解析、响应发送
- ✅ 从机 `mqtt_heartbeat.c` - 心跳发送
- ✅ 共享协议 `mqtt_iot_protocol.h` - Topic定义、数据包结构
- ✅ BSP层 `bsp_iot_controller.c` - 设备管理、命令构造

### 关键配置（当前使用公共Broker）
```c
// 主机和从机都使用：
#define MQTT_BROKER_URI  "mqtt://broker.emqx.io"
#define MQTT_BROKER_PORT 1883
```

### 控制链流程
```
主机语音命令 → MCP工具 → App_IOT_Set_Light/Relay/Servo()
    → IOT_Set_Remote_GPIO() → Mqtt_Publish()
    → Topic: "xiaozhi/iot/cmd/{device_id}"
    → Broker → 从机订阅该Topic → Process_Command()
    → 更新g_device_flags → iot_control_task刷新GPIO
    → Send_Response() → Topic: "xiaozhi/iot/resp/{mac}"
    → Broker → 主机订阅"xiaozhi/iot/resp/+" → 更新设备状态
```

---

## Task 1: 验证MQTT控制链完整性

**Files:**
- Read: `main/APP/Src/iot_controller.c`
- Read: `main/BSP/Src/bsp_iot_controller.c`
- Read: `shared/mqtt_iot_protocol.h`
- Read: `xiaozhi_slave_Firstfloor/main/APP/Src/mqtt_receive.c`
- Read: `xiaozhi_slave_Secondfloor/main/APP/Src/mqtt_receive.c`
- Read: `xiaozhi_slave_Thirdfloor/main/APP/Src/mqtt_receive.c`

- [ ] **Step 1: 验证主机MQTT初始化**

检查 `HW_MQTT_Init()`:
```c
esp_mqtt_client_config_t mqtt_cfg = {
    .broker.address.uri = "mqtt://broker.emqx.io",  // 公共Broker
    .broker.address.port = 1883,
    .credentials.client_id = "xiaozhi_master",
};
```

验收标准:
- 主机使用唯一Client ID "xiaozhi_master"
- 连接到 broker.emqx.io:1883
- 注册事件处理函数 `mqtt_event_handler`

- [ ] **Step 2: 验证主机订阅Topic**

检查 `mqtt_event_handler()` 中 MQTT_EVENT_CONNECTED:
```c
esp_mqtt_client_subscribe(s_mqtt_client, "xiaozhi/iot/resp/+", 0);
esp_mqtt_client_subscribe(s_mqtt_client, "xiaozhi/iot/heartbeat/+", 0);
esp_mqtt_client_subscribe(s_mqtt_client, "xiaozhi/iot/announce/+", 1);
```

验收标准:
- 订阅响应Topic通配符 `xiaozhi/iot/resp/+`
- 订阅心跳Topic通配符 `xiaozhi/iot/heartbeat/+`
- 订阅声明Topic通配符 `xiaozhi/iot/announce/+`

- [ ] **Step 3: 验证主机命令发送逻辑**

检查 `IOT_Set_Remote_GPIO()`:
```c
char topic[64];
snprintf(topic, sizeof(topic), MQTT_TOPIC_CMD_PREFIX "%d", dev->remote_devices[device_index].device_id);
// 生成: "xiaozhi/iot/cmd/1" (一楼), "xiaozhi/iot/cmd/2" (二楼), "xiaozhi/iot/cmd/3" (三楼)
bool result = dev->Mqtt_Publish(topic, (const uint8_t*)&cmd, sizeof(cmd));
```

验收标准:
- 根据device_id生成正确的单播Topic
- 一楼 → "xiaozhi/iot/cmd/1"
- 二楼 → "xiaozhi/iot/cmd/2"
- 三楼 → "xiaozhi/iot/cmd/3"

- [ ] **Step 4: 验证从机MQTT连接**

检查一楼从机 `MQTT_Receive_Init()`:
```c
#define MQTT_BROKER_URI     "mqtt://broker.emqx.io"
#define MQTT_CLIENT_ID      "xiaozhi_slave_1"
```

验收标准:
- 从机1 Client ID: "xiaozhi_slave_1"
- 从机2 Client ID: "xiaozhi_slave_2"
- 从机3 Client ID: "xiaozhi_slave_3"
- 都连接到同一个Broker

- [ ] **Step 5: 验证从机订阅Topic**

检查从机 `mqtt_event_handler()`:
```c
esp_mqtt_client_subscribe(s_mqtt_client, MQTT_TOPIC_CMD_BROADCAST, 1);
esp_mqtt_client_subscribe(s_mqtt_client, MQTT_TOPIC_CMD_PREFIX DEVICE_ID_FIRSTFLOOR, 1);
// 一楼订阅: "xiaozhi/iot/cmd/broadcast" 和 "xiaozhi/iot/cmd/1"
```

验收标准:
- 所有从机订阅广播Topic
- 一楼订阅 "xiaozhi/iot/cmd/1"
- 二楼订阅 "xiaozhi/iot/cmd/2"
- 三楼订阅 "xiaozhi/iot/cmd/3"

- [ ] **Step 6: 验证从机命令处理**

检查 `Process_Command()`:
```c
case IOT_CMD_SET_GPIO:
    if (cmd->gpio_index < LIGHT_COUNT) {
        g_device_flags.light[cmd->gpio_index] = (cmd->value == 1) ? ON : OFF;
        Send_Response(IOT_CMD_RESPONSE, cmd->gpio_index, cmd->value);
    }
```

验收标准:
- 收到命令后更新全局标志位
- 发送响应回主机
- 不直接操作GPIO（由iot_control_task处理）

- [ ] **Step 7: 验证心跳机制**

检查从机心跳:
```c
// 每30秒发送心跳
char topic[64];
snprintf(topic, sizeof(topic), MQTT_TOPIC_HEARTBEAT_PREFIX "%s", mac_str);
// Topic: "xiaozhi/iot/heartbeat/AABBCCDDEEFF"
```

验收标准:
- 心跳Topic包含MAC地址
- 主机从Topic解析MAC并更新心跳时间
- 超时60秒标记设备离线

---

## Task 2: 配置本地MQTT Broker（EMQX）

**Files:**
- Create: `docs/MQTT_Broker配置指南.md`
- Modify: `shared/mqtt_iot_protocol.h` (添加Broker配置宏)
- Modify: `main/APP/Src/iot_controller.c` (使用配置宏)
- Modify: `xiaozhi_slave_Firstfloor/main/APP/Src/mqtt_receive.c`
- Modify: `xiaozhi_slave_Secondfloor/main/APP/Src/mqtt_receive.c`
- Modify: `xiaozhi_slave_Thirdfloor/main/APP/Src/mqtt_receive.c`

- [ ] **Step 1: 下载并安装EMQX**

在Windows电脑上执行:
```powershell
# 1. 访问 https://www.emqx.io/zh/downloads
# 2. 下载 EMQX Broker for Windows (zip包)
# 3. 解压到 C:\emqx
```

验收标准:
- EMQX解压到 `C:\emqx`
- 目录包含 `bin\emqx.cmd`

- [ ] **Step 2: 启动EMQX Broker**

```powershell
cd C:\emqx\bin
.\emqx.cmd console
```

验收标准:
- 控制台显示 "EMQX Broker is running now."
- 访问 http://localhost:18083 显示登录页面

- [ ] **Step 3: 获取电脑局域网IP**

```powershell
ipconfig
# 找到 "无线局域网适配器 WLAN" 或 "以太网适配器"
# 记录 IPv4 地址，例如: 192.168.1.100
```

验收标准:
- 记录电脑的内网IP地址
- 确保ESP32设备和电脑在同一WiFi网络

- [ ] **Step 4: 修改共享协议头添加Broker配置**

修改 `shared/mqtt_iot_protocol.h`:
```c
// 在文件末尾添加
#ifndef MQTT_BROKER_URI
#define MQTT_BROKER_URI  "mqtt://broker.emqx.io"
#endif

#ifndef MQTT_BROKER_PORT
#define MQTT_BROKER_PORT 1883
#endif
```

验收标准:
- 添加可覆盖的Broker配置宏
- 默认仍使用公共Broker（向后兼容）

- [ ] **Step 5: 修改主机使用配置宏**

修改 `main/APP/Src/iot_controller.c`:
```c
esp_mqtt_client_config_t mqtt_cfg = {
    .broker.address.uri = MQTT_BROKER_URI,
    .broker.address.port = MQTT_BROKER_PORT,
    .credentials.client_id = "xiaozhi_master",
};
```

验收标准:
- 使用 `MQTT_BROKER_URI` 宏
- 使用 `MQTT_BROKER_PORT` 宏

- [ ] **Step 6: 修改从机使用配置宏**

修改3个从机的 `mqtt_receive.c`:
```c
esp_mqtt_client_config_t mqtt_cfg = {
    .broker.address.uri = MQTT_BROKER_URI,
    .broker.address.port = MQTT_BROKER_PORT,
    .credentials.client_id = MQTT_CLIENT_ID,
};
```

验收标准:
- 所有从机使用统一的Broker配置
- 每个从机保留各自的Client ID

- [ ] **Step 7: 创建本地Broker编译配置**

创建 `local_broker_config.h`:
```c
// 在编译时包含此文件以使用本地Broker
#undef MQTT_BROKER_URI
#define MQTT_BROKER_URI "mqtt://192.168.1.100"  // 修改为你的电脑IP
```

验收标准:
- 提供本地Broker配置模板
- 说明如何修改IP地址

---

## Task 3: 验证控制链端到端

**Files:**
- Test: 使用MQTTX或类似工具
- Read: `main/BSP/Inc/bsp_iot_controller.h`

- [ ] **Step 1: 验证设备发现流程**

使用MQTTX工具:
1. 连接到 `mqtt://broker.emqx.io:1883`
2. 订阅 `xiaozhi/iot/announce/+`
3. 发布到 `xiaozhi/iot/cmd/broadcast`
   Payload: `{0x06, 0, 0, 0, 0, 0, 0, 0}` (DISCOVER命令)
4. 观察是否收到3个从机的ANNOUNCE响应

验收标准:
- 收到3个设备的声明消息
- 每个消息包含正确的device_name和MAC

- [ ] **Step 2: 验证单播控制**

使用MQTTX:
1. 订阅 `xiaozhi/iot/resp/+`
2. 发布到 `xiaozhi/iot/cmd/1`
   Payload: `{0x01, 0x01, 0x00, 0x01, 0, 0, 0, 0}` (设置GPIO0=1)
3. 观察是否收到响应

验收标准:
- 收到一楼从机的响应
- 响应包含正确的gpio_index和value

- [ ] **Step 3: 验证广播控制**

使用MQTTX:
1. 订阅 `xiaozhi/iot/resp/+`
2. 发布到 `xiaozhi/iot/cmd/broadcast`
   Payload: `{0x32, 0, 0, 0, 0, 0, 0, 0}` (BROADCAST_LIGHTS_OFF)
3. 观察是否收到所有从机的响应

验收标准:
- 收到3个从机的响应（或至少确认命令被处理）

- [ ] **Step 4: 验证心跳机制**

使用MQTTX:
1. 订阅 `xiaozhi/iot/heartbeat/+`
2. 等待30秒
3. 观察是否收到心跳消息

验收标准:
- 每30秒收到一次心跳
- 心跳Topic包含MAC地址

---

## Task 4: 修复发现的问题

**Files:**
- Modify: 根据验证结果修复

- [ ] **Step 1: 修复Broker地址不一致问题**

如果主机和从机使用不同的Broker地址，统一为同一个。

- [ ] **Step 2: 修复Topic不匹配问题**

检查是否有拼写错误或不一致的Topic定义。

- [ ] **Step 3: 修复命令解析问题**

如果从机不响应某些命令，检查 `Process_Command()` 中的case处理。

- [ ] **Step 4: 修复心跳超时问题**

如果设备频繁标记为离线，检查心跳间隔和超时时间配置。

---

## 验证清单

### 控制链完整性
- [ ] 主机能连接MQTT Broker
- [ ] 从机能连接MQTT Broker
- [ ] 主机能发送命令到从机
- [ ] 从机能接收并执行命令
- [ ] 从机能发送响应回主机
- [ ] 主机能接收并处理响应
- [ ] 心跳机制正常工作
- [ ] 设备超时检测正常工作

### Broker配置
- [ ] EMQX安装在电脑上
- [ ] EMQX成功启动
- [ ] Dashboard可访问
- [ ] ESP32设备能连接到电脑IP
- [ ] 所有设备使用同一个Broker

### 端到端测试
- [ ] 设备发现正常
- [ ] 单播控制正常
- [ ] 广播控制正常
- [ ] 场景控制正常（回家/睡眠/起床）
- [ ] 紧急模式正常

---

## 已知限制

1. **公共Broker不稳定**: `broker.emqx.io` 可能随时不可用
2. **WiFi依赖**: 所有设备必须同一WiFi
3. **无加密**: 当前使用明文MQTT（端口1883）
4. **无认证**: 未配置用户名密码

## 生产环境建议

1. **使用本地Broker**: 电脑或路由器部署EMQX/Mosquitto
2. **启用认证**: 配置用户名密码
3. **启用TLS**: 使用端口8883加密通信
4. **监控Broker**: 使用EMQX Dashboard监控连接状态

---

*Plan version: 1.0*
*Created: 2026-04-20*
