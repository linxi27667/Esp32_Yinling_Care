# AI 工作日志 (ai_working_log.md)

本文档记录所有代码修改历史，确保项目可追溯。

---

## 2026-03-29 21:30（第十三次会话）

### 主题：三楼从机传感器系统重构 + 架构分析

### 问题描述

#### 问题1：DMA ADC 代码过于复杂
之前的 DMA 连续转换模式代码复杂，资源消耗高，对于 500ms 采样间隔来说杀鸡用牛刀。

#### 问题2：雨滴/烟雾传感器值不准确
- 雨滴传感器电压值持续偏低（< 20mV），显示"Heavy rain"
- 烟雾传感器值跳动大
- ADC 参考电压计算错误（1100mV 应该是 3100mV）

#### 问题3：ASR UART GPIO 冲突
- GPIO 43/44 被 Console UART0 占用
- ASR UART1 无法使用 GPIO 43

#### 问题4：链接错误
CMakeLists.txt 缺少 `bsp_asr_uart.c`，导致链接失败。

### 修复内容

#### 修复1：简化 ADC 为软件平均模式
```c
// 之前：DMA 连续转换（复杂）
#include "esp_adc/adc_continuous.h"
adc_continuous_handle_t g_adc_handle = NULL;
adc_continuous_read(g_adc_handle, buf, ...);

// 现在：软件16次平均（简单稳定）
static int32_t ADC_Get_Voltage_MV(adc1_channel_t channel) {
    int sum = 0;
    for (int i = 0; i < 16; i++) {
        sum += adc1_get_raw(channel);
    }
    return (sum * 3100) / (4095 * 16);  // 3100mV 参考电压
}
```

#### 修复2：ASR UART 改为 GPIO 17
```c
// app_asr_uart.c
#define ASR_UART_TX_PIN    GPIO_NUM_17  // 之前是 GPIO_NUM_43
```

#### 修复3：添加 bsp_asr_uart.c 到 CMakeLists.txt
```cmake
idf_component_register(SRCS ...
                            "BSP/Src/bsp_asr_uart.c"  # 新增
                        PRIV_REQUIRES ...)
```

### 新增功能

#### DEBUG_MODE 5秒传感器日志
```c
#if DEBUG_MODE == 1
    static uint32_t last_log = 0;
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (now - last_log >= 5000) {
        ESP_LOGI(TAG, "[DEBUG] Smoke: %ld mV, Rain: %ld mV, Help: %d",
                 smoke_mv, rain_mv, help_btn);
        ESP_LOGI(TAG, "[DEBUG] Status -> Fire: %d, Rain: %d, Help: %d",
                 g_device_flags.fire_status,
                 g_device_flags.rain_status,
                 g_device_flags.help_status);
        last_log = now;
    }
#endif
```

### 传感器阈值配置
| 传感器 | 阈值 | 说明 |
|--------|------|------|
| 烟雾 | >= 1500mV | 60% 触发火灾报警 |
| 雨滴-干燥 | > 2640mV | >80% 干燥 |
| 雨滴-小雨 | 1320-2640mV | 40-80% |
| 雨滴-大雨 | < 1320mV | <40% 触发收衣服 |

### 修改文件清单

| 文件 | 修改内容 |
|------|----------|
| `xiaozhi_slave_Thirdfloor/main/APP/Src/sensor_adc_task.c` | DMA→软件平均，16次采样 |
| `xiaozhi_slave_Thirdfloor/main/APP/Src/app_asr_uart.c` | GPIO 43→GPIO 17 |
| `xiaozhi_slave_Thirdfloor/main/APP/Inc/sensor_config.h` | 简化配置，移除DMA相关 |
| `xiaozhi_slave_Thirdfloor/main/CMakeLists.txt` | 添加 bsp_asr_uart.c |

### 新增文档

| 文件 | 说明 |
|------|------|
| `xiaozhi_slave_Thirdfloor/三楼从机接线文档.md` | 完整接线图、传感器阈值、通信协议 |

### 待办事项
- [ ] 实际测试雨滴传感器（干燥状态应 > 2640mV）
- [ ] 实际测试烟雾传感器（打火机靠近时 > 1500mV）
- [ ] 验证 ASR 语音报警（GPIO17 连接正确）

---

## 2026-03-27 14:30（第十二次会话）

### 主题：set_servo_by_index 不工作 Bug 深度修复

### 问题描述

**现象**：
- `all_on`/`all_off` 可以控制舵机
- `set_servo_by_index` 发送命令后舵机不动
- 日志显示命令接收到了：`ESP_NOW_RECV: Command: cmd=16, gpio=6, value=90`

### 根本原因

**两个独立问题**：

#### 问题 1：枚举值 vs 实际角度值混淆
- `all_on` 设置 `g_device_flags.servo[i] = SERVO_90`（枚举值 = 2）
- `set_servo_by_index` 设置 `g_device_flags.servo[i] = 90`（实际角度值）
- 但 `g_servo_angle_map[2] = 90°` ✅ 而 `g_servo_angle_map[90]` = 越界 ❌

#### 问题 2：gpio_index 验证逻辑错误
- Master 发送 `gpio_index=6,7,8`（全局 GPIO 索引）
- Slave 检查 `gpio_index < SERVO_COUNT (3)`，即 `6 < 3 = FALSE`
- **命令被直接拒绝！**

### 修复内容

**修复 1**：角度值→枚举转换
```c
// 修复前（错误）
g_device_flags.servo[cmd->gpio_index] = (servo_angle_enum_t)cmd->value;

// 修复后（正确）- 角度值除以45得到枚举索引
g_device_flags.servo[cmd->gpio_index] = (servo_angle_enum_t)(cmd->value / 45);
```

**修复 2**：gpio_index 范围检查
```c
// 修复前（错误）
if (cmd->gpio_index < SERVO_COUNT && cmd->value % 45 == 0 && cmd->value <= 180)

// 修复后（正确）- 舵机GPIO索引是6,7,8
if (cmd->gpio_index >= 6 && cmd->gpio_index < 6 + SERVO_COUNT &&
    cmd->value % 45 == 0 && cmd->value <= 180) {
    uint8_t servo_local_index = cmd->gpio_index - 6;  // 转换为本地索引0,1,2
```

### 修改文件清单

| 文件 | 修改 |
|------|------|
| `xiaozhi_slave_Firstfloor/main/APP/Src/esp_now_receive.c` | gpio_index验证修复 + 角度→枚举转换 |
| `xiaozhi_slave_Secondfloor/main/APP/Src/esp_now_receive.c` | gpio_index验证修复 + 角度→枚举转换 |
| `xiaozhi_slave_Thirdfloor/main/APP/Src/esp_now_receive.c` | gpio_index验证修复 + 角度→枚举转换 |

### 验证结果

修复后日志：
```
I (68536) ESP_NOW_RECV: Servo[0] (GPIO6) = 90 deg   ← 正常工作
I (70972) ESP_NOW_RECV: Servo[1] (GPIO7) = 90 deg
I (73644) ESP_NOW_RECV: Servo[2] (GPIO8) = 90 deg
```

---

## 2026-03-27 11:45（第十一次会话）

### 主题：DEBUG_MODE 修复 + 按键角度循环 Bug 修复

### 问题描述

#### Bug 1：DEBUG_MODE==1 时小智无法控制舵机
**现象**：按键调试正常，但小智发送 ESP-NOW 舵机命令时舵机不动

**日志**：
```
I (310761) ESP_NOW_RECV: Command: cmd=16, gpio=6, value=90
I (322971) ESP_NOW_RECV: Command: cmd=16, gpio=6, value=180
```

**根本原因**：`#if DEBUG_MODE != 1` 把 ESP-NOW 舵机刷新路径完全禁用

```c
// 错误代码 - DEBUG_MODE==1 时这段代码被注释掉
for (int i = 0; i < SERVO_COUNT; i++) {
#if DEBUG_MODE != 1
    HW_Servo_Set_Angle(g_servo_channels[i], g_servo_angle_map[g_device_flags.servo[i]]);
#endif
}
```

#### Bug 2：按键角度循环 0→135 跳不到 180→0
**现象**：0→45→90→135 正常，但 135 后跳到 45 而非 0

**根本原因**：`% 180` 模数错误
- `(180 + 45) % 180 = 45` ❌ 应该是 0

### 修复内容

**修复 1**：移除 ESP-NOW 舵机刷新的 DEBUG_MODE 条件编译

```c
// 修复后 - ESP-NOW 舵机控制始终启用
for (int i = 0; i < SERVO_COUNT; i++) {
    HW_Servo_Set_Angle(g_servo_channels[i], g_servo_angle_map[g_device_flags.servo[i]]);
}
```

**修复 2**：角度循环改用 % 225（LCM of 45 and 180）

```c
// 修复后 - 0→45→90→135→180→0 正确循环
g_debug_servo_current_angle = (g_debug_servo_current_angle + DEBUG_STEP_DEGREES) % 225;
```

### DEBUG_MODE 设计原则
- **按键控制**：DEBUG_MODE==1 时启用 GPIO47 按键控制舵机
- **小智控制**：始终启用，不受 DEBUG_MODE 影响
- **两者共存**：按键和小智可以同时工作

### 修改文件清单

| 文件 | 修改 |
|------|------|
| `xiaozhi_slave_Firstfloor/main/APP/Src/iot_control_task.c` | 移除 `#if DEBUG_MODE != 1` 保护；`% 225` 修复 |
| `xiaozhi_slave_Secondfloor/main/APP/Src/iot_control_task.c` | 移除 `#if DEBUG_MODE != 1` 保护；`% 225` 修复 |
| `xiaozhi_slave_Thirdfloor/main/APP/Src/iot_control_task.c` | 移除 `#if DEBUG_MODE != 1` 保护；`% 225` 修复 |

### 角度循环验证
| 当前角度 | +45 后（%225） | 正确？ |
|---------|---------------|--------|
| 0 | 45 | ✅ |
| 45 | 90 | ✅ |
| 90 | 135 | ✅ |
| 135 | 180 | ✅ |
| 180 | 0 | ✅ |
| 225 | 45 | ✅（回到起点） |

---

## 2026-03-27 10:30（第十次会话）

### 主题：180° 舵机修复 + 药盒 270° 保持不变

### 问题描述
用户控制一楼/二楼/三楼的舵机时，示波器测量有 PWM 信号但舵机不转动。

### 根本原因
**舵机角度参数不匹配**：
- 一楼/二楼/三楼使用 **180° 舵机**
- 代码按 **270° 舵机（药盒）** 参数计算
- 180° 舵机期望脉宽：500µs(0°) ~ 2500µs(180°)，中值 1500µs = 90°
- 旧代码给 90° 时，发送脉宽约 1667µs，舵机只转到约 67.5°，力量不够

### 修复内容

**修改文件**（一楼/二楼/三楼共6个文件）：
- `xiaozhi_slave_Firstfloor/main/APP/Src/iot_control_task.c`
- `xiaozhi_slave_Firstfloor/main/BSP/Inc/bsp_servo.h`
- `xiaozhi_slave_Secondfloor/main/APP/Src/iot_control_task.c`
- `xiaozhi_slave_Secondfloor/main/BSP/Inc/bsp_servo.h`
- `xiaozhi_slave_Thirdfloor/main/APP/Src/iot_control_task.c`
- `xiaozhi_slave_Thirdfloor/main/BSP/Inc/bsp_servo.h`

**180° 舵机参数**：
```c
#define SERVO_MIN_ANGLE    0.0f      // 原 -135.0f
#define SERVO_MAX_ANGLE    180.0f    // 原 135.0f（180°舵机角度范围 0-180°）
```

**180° 舵机脉宽公式**：
```c
// 旧公式（270°）：pulse = 500 + (angle + 135) * 2000 / 270
// 新公式（180°）：pulse = 500 + angle * 2000 / 180
```

**药盒（270°）保持不变** ✅：
```c
#define SERVO_MIN_ANGLE    -135.0f
#define SERVO_MAX_ANGLE    135.0f
```

### 修复后的 PWM 行为
| 目标角度 | 180° 舵机脉宽 | 270° 舵机（药盒）脉宽 |
|---------|--------------|---------------------|
| 0° | 500µs | 500µs |
| 90° | 1500µs | 1500µs |
| 180° | 2500µs | 2500µs |

### 🔴 关键 Bug 修复：LEDC 通道冲突

**问题**：舵机 GPIO 有 PWM 信号但舵机不转，示波器测量波形不变。

**根本原因**：所有 3 个舵机都使用同一个 `LEDC_CHANNEL_0`！
```c
// 错误代码
ledc_channel_config_t channel = {
    .channel = LEDC_CHANNEL_0,  // ❌ 三个舵机都用通道0
    .gpio_num = gpio,           // 最后一个 GPIO40 覆盖了前面的
};
```
**结果**：只有 GPIO40 有 PWM，GPIO18 和 GPIO21 被 GPIO40 覆盖！

**修复方案**：每个舵机使用独立 LEDC 通道
```c
static ledc_channel_t g_servo_channels[SERVO_COUNT] = {
    LEDC_CHANNEL_0, LEDC_CHANNEL_1, LEDC_CHANNEL_2
};

static void HW_Servo_Init(gpio_num_t gpio, ledc_channel_t channel);
static void HW_Servo_Set_Angle(ledc_channel_t channel, int16_t angle);
```

**修改文件**：
- `xiaozhi_slave_Firstfloor/main/APP/Src/iot_control_task.c`
- `xiaozhi_slave_Secondfloor/main/APP/Src/iot_control_task.c`
- `xiaozhi_slave_Thirdfloor/main/APP/Src/iot_control_task.c`

---

## 2026-03-26 21:00（第九次会话）

### 主题：中文设备名称绑定 + 意图识别问题分析

### 完成内容

#### 1. 新增 MCP 工具：all_lights_on / all_lights_off

**问题**：用户说"打开所有灯"时，小智错误调用 `all_on`（打开所有设备），应该只打开灯。

**解决方案**：添加专门控制灯的 MCP 工具。

**新增 ESP-NOW 命令**：
```c
IOT_CMD_BROADCAST_LIGHTS_OFF = 0x32  // 只关灯
IOT_CMD_BROADCAST_LIGHTS_ON  = 0x33  // 只开灯
```

**修改文件汇总**：

| 层级 | 文件 | 修改 |
|------|------|------|
| 协议层 | `main/BSP/Inc/bsp_iot_controller.h` | 添加命令枚举 |
| BSP 层 | `main/BSP/Src/bsp_iot_controller.c` | 添加 `IOT_Broadcast_Lights_On/Off()` |
| APP 层 | `main/APP/Inc/iot_controller.h` | 添加函数声明 |
| APP 层 | `main/APP/Src/iot_controller.c` | 添加 `App_IOT_Broadcast_Lights_On/Off()` |
| MCP 层 | `main/boards/custom-esp32s3-cam/custom_esp32s3_cam.cc` | 添加 `self.iot.all_lights_on/off` |
| 从机 | `xiaozhi_slave_Firstfloor/.../esp_now_receive.c` | 添加命令处理 |
| 从机 | `xiaozhi_slave_Secondfloor/.../esp_now_receive.c` | 添加命令处理 |
| 从机 | `xiaozhi_slave_Thirdfloor/.../esp_now_receive.c` | 添加命令处理 |
| 从机 | `xiaozhi_slave_yaohe/.../esp_now_receive.c` | 添加命令处理 |

**MCP 工具说明**：

| 工具 | 触发场景 | 功能 |
|------|---------|------|
| `self.iot.all_lights_on` | "开灯"、"打开所有灯" | 只开灯，不影响继电器/舵机 |
| `self.iot.all_lights_off` | "关灯"、"关闭所有灯" | 只关灯，不影响继电器/舵机 |
| `self.iot.all_on` | "打开所有设备" | 开灯+继电器+舵机 |
| `self.iot.all_off` | "关闭所有设备" | 关灯+继电器+舵机 |

---

#### 2. 中文设备名称宏（用于 MCP 意图识别）

**新增宏定义**（`main/APP/Inc/iot_controller.h`）：
```c
#define CHINESE_LIGHT_NAMES(n) \
    "一楼设备:浴室灯/客厅灯/卧室灯; " \
    "二楼设备:二楼浴室灯/二楼客厅灯/二楼卧室灯; " \
    "三楼设备:三楼浴室灯/三楼客厅灯/三楼卧室灯"

#define CHINESE_RELAY_NAMES(n) \
    "一楼设备:电视电源/空调电源/热水器电源; " \
    "二楼设备:二楼电视电源/二楼空调电源/二楼热水器电源; " \
    "三楼设备:三楼电视电源/三楼空调电源/三楼热水器电源"

#define CHINESE_SERVO_NAMES(n) \
    "一楼设备:窗户/窗帘/门锁; " \
    "二楼设备:二楼窗户/二楼窗帘/二楼门锁; " \
    "三楼设备:三楼窗户/三楼窗帘/三楼门锁"
```

**MCP 工具使用宏**（`custom_esp32s3_cam.cc`）：
```cpp
mcp_server.AddTool("self.iot.set_light", ..., "中文灯名: " CHINESE_LIGHT_NAMES());
mcp_server.AddTool("self.iot.set_relay", ..., "中文继电器名: " CHINESE_RELAY_NAMES());
mcp_server.AddTool("self.iot.set_servo_by_index", ..., "中文舵机名: " CHINESE_SERVO_NAMES());
```

**效果**：AI 模型现在能识别用户说的"关浴室灯"、"打开二楼客厅灯"等中文别名。

---

#### 3. 意图识别问题 - 已通过 all_lights_on/off 解决

**原问题**："关闭所有灯" 会触发 "关闭所有设备"
**解决方案**：添加 `self.iot.all_lights_on/off` 专门处理灯的控制

---

#### 4. AudioCodec 卡死问题（暂不影响演示）

**日志**：`I (95489) AudioCodec: Set output enable to false`

**分析**：音频电源管理逻辑，暂不影响演示。

---

## 2026-03-26 20:30（第八次会话）

### 主题：二楼/三楼/药盒设备 V2 协议升级 + 继电器控制修复

### 问题背景

一楼设备已完成 V2 协议升级，但二楼、三楼和药盒设备仍使用旧代码，导致：
- 二楼/三楼：Peer exists 警告 + 无法正常响应主机
- 药盒：使用 V1 协议，主机显示"未知设备"

---

### 修复内容

#### 1. 主机 IOT_Set_Relay gpio_index 计算错误（严重！）

**问题**：`IOT_Set_Relay()` 直接用 `relay_index` 作为 `gpio_index`，导致继电器被当成灯处理。

**修复文件**：`main/bsp/src/bsp_iot_controller.c`

```c
// 修复前（错误）
int gpio_index = relay_index;  // relay_index=0 → gpio_index=0（灯）

// 修复后（正确）
int gpio_index = 3 + relay_index;  // relay_index=0 → gpio_index=3（继电器1）
```

#### 2. 主机 IOT_Set_Servo gpio_index 计算错误

```c
// 修复后
int gpio_index = 6 + servo_index;  // servo_index=0 → gpio_index=6（舵机1）
```

#### 3. 二楼/三楼设备 esp_now_receive.c 升级

将一楼设备的完整 V2 协议代码应用到二楼和三楼：

**修复内容**：
- 添加 `Get_Current_Wifi_Channel()` 动态获取信道
- `Send_Response()` 添加 peer 检查（`esp_now_is_peer_exist` → `esp_now_mod_peer/add_peer`）
- `Send_Announce()` 发送 V2 协议包（包含设备名称）
- `EspNow_Receive_Init()` 获取 WiFi 信道

**设备名称**：
- 二楼：`#define DEVICE_NAME "二楼设备"`
- 三楼：`#define DEVICE_NAME "三楼设备"`

#### 4. 智能药盒添加 V2 协议支持（保留原有功能）

**修复文件**：`xiaozhi_slave_yaohe/main/APP/Src/esp_now_receive.c`

**关键修改**：
1. 添加 `iot_announce_v2_packet_t` 结构体和 `IOT_CMD_ANNOUNCE_V2 = 0x08` 命令
2. 添加 `Send_Announce_V1()` 和 `Send_Announce_V2()` 两个函数
3. `IOT_CMD_DISCOVER` 处理中调用 `Send_Announce_V2()`
4. 保留所有原有命令：`IOT_CMD_OPEN_MEDICINE_BOX`、`IOT_CMD_SET_SERVO` 等
5. 添加 peer 存在性检查（`esp_now_is_peer_exist` → `esp_now_mod_peer/add_peer`）

**设备名称**：`#define DEVICE_NAME "智能药盒"`

---

### 修复后验证

#### 编译后检查日志

**一楼/二楼/三楼设备**应显示：
```
ESP_NOW_RECV: WiFi connected, ESP-NOW will use channel X
ESP_NOW_RECV: Announce V2 sent: 一楼设备 (channel X)
ESP_NOW_RECV: Command: cmd=1, gpio=3, value=1  ← 继电器控制
```

**智能药盒**应显示：
```
ESP_NOW_RECV: Announce V2 sent: 智能药盒 (channel X)
```

#### 主机端验证

主机应能识别所有四个设备：
- 一楼设备
- 二楼设备
- 三楼设备
- 智能药盒

---

## 2026-03-26 19:30（第七次会话）

### 主题：ESP-NOW 信道不匹配问题修复 + 从机无法通信根因定位

### 问题1：ESP-NOW 信道不匹配（严重！）

**日志证据**：
```
主机日志: ESP-NOW initialized (channel 8)
从机日志: ESP-NOW initialized on channel 1  ← 信道不匹配！
```

**根本原因**：
- 主机 ESP-NOW 跟随 WiFi 信道（channel 8）
- 从机硬编码 `#define ESPNOW_CHANNEL 1`
- **信道不匹配 = 从机根本收不到主机的任何命令！**

### 问题2：从机未添加 peer

从机发送 ANNOUNCE_V2 响应给主机前，没有调用 `esp_now_add_peer()`，导致发送失败。

---

### 修复方案

#### 1. 从机 ESP-NOW 初始化时获取当前 WiFi 信道

**修改文件**：`xiaozhi_slave_Firstfloor/main/APP/Src/esp_now_receive.c`

```c
// 修复前
#define ESPNOW_CHANNEL  1
static uint8_t g_espnow_channel = ESPNOW_CHANNEL;

// 修复后
static uint8_t g_espnow_channel = 0;

// 初始化时获取实际信道
wifi_ap_record_t ap_info;
if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
    g_espnow_channel = ap_info.primary;
}
```

#### 2. 添加 Get_Current_Wifi_Channel() 辅助函数

```c
static uint8_t Get_Current_Wifi_Channel(void) {
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        return ap_info.primary;
    }
    return ESPNOW_CHANNEL;
}
```

#### 3. 发送响应前添加 peer

```c
static void Send_Announce(const uint8_t* dest_mac) {
    // ... 构造响应包 ...

    uint8_t channel = Get_Current_Wifi_Channel();
    esp_now_peer_info_t peer = {
        .channel = channel,
        .ifidx = WIFI_IF_STA,
        .encrypt = false,
    };
    memcpy(peer.peer_addr, dest_mac, 6);
    esp_now_add_peer(&peer);
    esp_now_send(dest_mac, (const uint8_t*)&resp, sizeof(resp));
}
```

---

### 本次修改文件汇总

| 文件 | 修改内容 |
|------|----------|
| `xiaozhi_slave_Firstfloor/main/APP/Src/esp_now_receive.c` | 1. 信道跟随 WiFi 2. 添加 Get_Current_Wifi_Channel() 3. 发送前添加 peer |

### 待验证

重新烧录从机后，检查日志是否显示：
```
ESP_NOW_RECV: WiFi connected, ESP-NOW will use channel X
ESP_NOW_RECV: Announce V2 sent: 一楼设备 (channel X)
```

---

## 2026-03-26 18:30（第六次会话）

### 主题：从机编译成功 + 设备名协议验证 + 工作日志整理

### 问题1：编译错误 - 枚举数组索引越界

**错误信息**：
```
error: array index in initializer exceeds array bounds
    [SERVO_45]  = SERVO_ANGLE_45,
```

**根本原因**：
枚举定义使用了角度值作为枚举值：
```c
typedef enum {
    SERVO_0   = 0,
    SERVO_45  = 45,   // 错误！数组大小只有4，最大索引为3
    SERVO_90  = 90,
    SERVO_135 = 135
} servo_angle_enum_t;
```

**修复方案**：
改为索引值：
```c
typedef enum {
    SERVO_0   = 0,
    SERVO_45  = 1,
    SERVO_90  = 2,
    SERVO_135 = 3
} servo_angle_enum_t;
```

**修改文件**：
- `xiaozhi_slave_Firstfloor/main/APP/Inc/iot_control_task.h`

---

### 问题2：从机日志疯狂打印

**原因**：`Device_Refresh_Task` 每 100ms 打印一次舵机角度

**修复方案**：
移除 `HW_Servo_Set_Angle` 函数中的调试日志

**修改文件**：
- `xiaozhi_slave_Firstfloor/main/APP/Src/iot_control_task.c`

```c
// 移除的代码
ESP_LOGI(TAG, "Servo GPIO%d: %d deg", gpio, angle);
```

---

### 问题3：从机发送 V2 协议但主机显示"未知设备"

**验证结果**：
- ✅ 从机日志确认：`Announce V2 sent: 一楼设备`
- ❌ 主机日志显示：`叫" 未知设备"`

**结论**：
- 从机代码正确，使用 `IOT_CMD_ANNOUNCE_V2` 协议
- 主机代码需要重新编译以处理 V2 协议的设备名更新

**解决方案**：
重新编译并烧录主机（`xiaozhi-esp32-main`）

---

### 本次修改文件汇总

#### 从机 firstfloor
| 文件 | 修改内容 |
|------|----------|
| `xiaozhi_slave_Firstfloor/main/APP/Inc/iot_control_task.h` | 修复枚举值：45→1, 90→2, 135→3 |
| `xiaozhi_slave_Firstfloor/main/APP/Src/iot_control_task.c` | 移除舵机调试日志 |

#### 主机 BSP 层（第五次会话遗留）
| 文件 | 修改内容 |
|------|----------|
| `main/BSP/Src/bsp_iot_controller.c` | 新增缺失函数（见第五次会话） |

---

## 2026-03-26 15:30（第五次会话）

### 主题：链接错误修复 + 设备名称"未知设备"问题排查

### 本次修改文件

#### 主机 BSP 层
| 文件 | 修改内容 |
|------|----------|
| `main/BSP/Src/bsp_iot_controller.c` | 新增函数：`IOT_Controller_Init()`、`IOT_Set_Servo()`、`IOT_Broadcast_All_Off()`、`IOT_Broadcast_All_On()`、`IOT_Check_Device_Timeout()`、`IOT_Open_Medicine_Box()` |

### 编译错误修复记录

#### 错误1：函数签名不匹配
```
error: conflicting types for 'IOT_Set_Light'
```
**原因**：函数参数类型不一致。头文件声明 `uint8_t light_index`，实现写成 `int light_index`
**修复**：将 `int light_index` 改为 `uint8_t light_index`

#### 错误2：未定义引用
```
undefined reference to `IOT_Set_Servo'
undefined reference to `IOT_Broadcast_All_Off'
undefined reference to `IOT_Broadcast_All_On'
```
**修复**：在 `bsp_iot_controller.c` 末尾添加缺失的函数实现

#### 错误3：`IOT_Set_Relay` 内部 bug
**原因**：函数内部使用 `light_index` 而不是 `relay_index`
```c
// 修复前（错误）
int gpio_index = light_index;

// 修复后（正确）
int gpio_index = relay_index;
```

---

### ⚠️ 重要问题：设备名称"未知设备"

#### 问题现象
从机发送 `cmd=7`（旧协议 `IOT_CMD_ANNOUNCE`），主机收到后创建设备名为"未知设备"

#### 日志证据
```
I (11229) APP_IOT: ESP-NOW received 8 bytes from 80:65:99:DF:C0:E0
I (11239) BSP_IOT: Process_Remote_Response: mac=80:65:99:DF:C0:E0, cmd=7, found=-1, len=8
Added remote device: 未知设备 (id=1, gpios=4)
```

#### 根本原因
从机发送的是 **cmd=7**（`IOT_CMD_ANNOUNCE`），但代码中 `Send_Announce()` 使用的是 `IOT_CMD_ANNOUNCE_V2`（cmd=8）。这说明**从机固件没有更新**，仍在运行旧代码。

#### 从机代码分析
```c
// esp_now_receive.c 第80-88行
static void Send_Announce(const uint8_t* dest_mac) {
    iot_announce_v2_packet_t resp = {
        .command = IOT_CMD_ANNOUNCE_V2,  // cmd=8
        ...
        .device_name = DEVICE_NAME        // "一楼设备"
    };
    esp_now_send(dest_mac, (const uint8_t*)&resp, sizeof(resp));
    ESP_LOGI(TAG, "Announce V2 sent: %s", DEVICE_NAME);
}
```

#### 解决方案
**重新编译并烧录所有从机**（一楼/二楼/三楼设备），确保使用最新代码。

---

## 2026-03-26 14:00（第四次会话）

### 新增功能：三楼层设备控制 + 小智意图识别

### 背景
用户将药盒代码复制为三份（一楼/二楼/三楼设备），要求：
- 移除药盒意图，保留舵机驱动
- 每层设备：3 灯 + 3 继电器 + 3 舵机
- 小智能通过 MCP 控制所有设备
- 支持"一键全开/全关"意图

### 修改文件清单

#### 主机 BSP 层
| 文件 | 修改内容 |
|------|----------|
| `main/BSP/Inc/bsp_iot_controller.h` | 新增命令枚举：`IOT_CMD_SET_LIGHT`(0x11)、`IOT_CMD_SET_RELAY`(0x12)、`IOT_CMD_BROADCAST_ALL_OFF`(0x30)、`IOT_CMD_BROADCAST_ALL_ON`(0x31) |
| `main/BSP/Src/bsp_iot_controller.c` | 新增函数：`IOT_Set_Light()`、`IOT_Set_Relay()`、`IOT_Set_Servo()`、`IOT_Broadcast_All_Off()`、`IOT_Broadcast_All_On()` |

#### 主机 APP 层
| 文件 | 修改内容 |
|------|----------|
| `main/APP/Inc/iot_controller.h` | 新增函数声明：`App_IOT_Set_Light()`、`App_IOT_Set_Relay()`、`App_IOT_Set_Servo_ByIndex()`、`App_IOT_Broadcast_All_Off()`、`App_IOT_Broadcast_All_On()` |
| `main/APP/Src/iot_controller.c` | 新增函数实现 |

#### MCP 工具注册
| 文件 | 新增工具 |
|------|----------|
| `main/boards/custom-esp32s3-cam/custom_esp32s3_cam.cc` | `self.iot.set_light`、`self.iot.set_relay`、`self.iot.set_servo_by_index`、`self.iot.all_off`、`self.iot.all_on` |

### 从机代码结构（三份）
```
xiaozhi_slave_Firstfloor/   ← 一楼设备
xiaozhi_slave_Secondfloor/  ← 二楼设备
xiaozhi_slave_Thirdfloor/   ← 三楼设备
```

每份包含：
- `iot_control_task.h` - 设备标志位枚举定义
- `iot_control_task.c` - GPIO 定义 + Device_Refresh_Task
- `esp_now_receive.c` - 命令解析（处理 IOT_CMD_BROADCAST_ALL_ON/OFF）

### MCP 工具接口

| MCP 工具 | 参数 | 功能 |
|----------|------|------|
| `self.iot.set_light` | device, index(0-2), value(true/false) | 控制灯光 |
| `self.iot.set_relay` | device, index(0-2), value(true/false) | 控制继电器 |
| `self.iot.set_servo_by_index` | device, index(0-2), angle(0/45/90/135) | 控制舵机角度 |
| `self.iot.all_off` | (无) | **一键全关** |
| `self.iot.all_on` | (无) | **一键全开** |

### 广播命令流程
```
小智语音: "关闭所有设备"
    ↓
触发 MCP 工具: self.iot.all_off
    ↓
App_IOT_Broadcast_All_Off() → IOT_Broadcast_All_Off()
    ↓
ESP-NOW 广播 (FF:FF:FF:FF:FF:FF)
    ↓
所有从机收到 IOT_CMD_BROADCAST_ALL_OFF
    ↓
设置 g_device_flags 全为 OFF/默认值
    ↓
Device_Refresh_Task() 每 100ms 刷新 GPIO
```

### 架构优点
1. **标志位驱动**：意图简单 → 标志位简单 → 执行简单
2. **广播一次，全设备响应**：无需遍历设备列表
3. **枚举类型安全**：灯/继电器用 ON/OFF，舵机用 0/45/90/135

---

## 2026-03-25 21:15（第三次会话）

### 问题诊断：药盒控制"离线"Bug修复

### 故障现象
- 用户说"关闭药盒"时，小智回复"智能药盒好像离线了，关闭失败呢"
- Terminal显示 `Remote device 智能药盒 is offline`
- 实际上从机正常运行，只是主机误判为离线

### 根本原因
主机 BSP 层的三个函数在检测到设备 `offline` 时直接 `return false`，导致 **ESP-NOW 命令根本没有发送出去**。

ESP-NOW 是 UDP 性质的广播协议，即使设备被标记为"offline"，命令仍然可以发送出去！当前的离线检查反而阻止了命令发送。

### 问题代码位置
| 函数 | 行号 | 问题 |
|------|------|------|
| `IOT_Set_Remote_GPIO` | 146-150 | offline 时 `return false` |
| `IOT_Set_Remote_Servo` | 333-337 | offline 时 `return false` |
| `IOT_Open_Medicine_Box` | 372-376 | offline 时 `return false` |

### 修复方案
将 `Log_Error` + `return false` 改为 `Log_Warn` + 继续发送命令（不移除offline检查，仅改变日志级别）。

### 修改文件
- `main/BSP/Src/bsp_iot_controller.c`

### 架构变更
**修复前**：
```c
if (!dev->remote_devices[device_index].online) {
    if (dev->Log_Error) {
        dev->Log_Error(TAG, "Remote device %s is offline", ...);
    }
    return false;  // 命令根本没发出去！
}
```

**修复后**：
```c
if (!dev->remote_devices[device_index].online) {
    if (dev->Log_Warn) {
        dev->Log_Warn(TAG, "Remote device %s is offline, trying anyway...", ...);
    }
    // 继续发送命令
}
```

### 待优化
- [ ] 从机心跳间隔从 30s 缩短到 10s，提高响应灵敏度
- [ ] 主机离线判断阈值从 60s 缩短到 20s

---

## 2026-03-25 20:30（第二次会话）

### 架构演进：从裸 uint8_t → 枚举类型

### 背景
用户要求所有设备状态使用枚举而非裸 `uint8_t`，包括：
- 灯/继电器：ON/OFF 两种枚举状态
- 药盒舵机：7 种枚举状态（MED_BOX_CLOSE ~ MED_BOX_6）

### 新增修改文件
- `xiaozhi_slave_yaohe/main/APP/Src/iot_control_task.c` - 重构为枚举架构
- `xiaozhi_slave_yaohe/main/APP/Inc/iot_control_task.h` - 新增枚举定义
- `xiaozhi_slave_yaohe/main/APP/Src/esp_now_receive.c` - 使用枚举更新标志位
- `main/BSP/Src/bsp_iot_controller.c` - 主机创建设备时名字改为 "智能药盒"

### 架构变更
**设备状态枚举：**
```c
typedef enum { LIGHT_OFF = 0, LIGHT_ON = 1 } light_state_enum_t;
typedef enum { MED_BOX_CLOSE = 0, MED_BOX_1 = 1, MED_BOX_2 = 2,
              MED_BOX_3 = 3, MED_BOX_4 = 4, MED_BOX_5 = 5, MED_BOX_6 = 6 } med_box_state_enum_t;

typedef struct {
    light_state_enum_t light_state;
    med_box_state_enum_t box_state;
} device_flags_enum_t;
```

**枚举 -> 角度映射表：**
```c
static const int16_t g_box_angle_map[7] = {
    [MED_BOX_CLOSE] = 99,   // 关闭
    [MED_BOX_1] = 63,       // 药盒1
    [MED_BOX_2] = 29,       // 药盒2
    [MED_BOX_3] = -8,       // 药盒3
    [MED_BOX_4] = -45,      // 药盒4
    [MED_BOX_5] = -81,      // 药盒5
    [MED_BOX_6] = -110      // 药盒6
};
```

### Bug修复
- **栈溢出**：`debug_btn` 任务连续 3 次 `ESP_LOGI` + `float` 运算导致栈溢出 → 删减日志输出
- **舵机角度被覆盖**：调试按钮更新了舵机但没更新 `g_device_flags.box_state`，导致 `device_refresh_task` 每 100ms 覆盖回 99° → 调试按钮现在同步更新 `g_device_flags.box_state`

### 待办事项
- [x] 从机编译验证
- [x] 从机实际上电测试
- [ ] 主机端 ESP-NOW 通信测试
- [ ] 主机端 MCP 工具新增场景化命令
- [ ] 三楼层从机 GPIO 规划
- [ ] **⚠️ 主机名称广播协议**：从机需要知道主机名称，建议新增 `IOT_CMD_MASTER_ANNOUNCE` 命令
- [ ] **⚠️ 从机保存主机 MAC**：从机启动后自动连接已知主机，无需每次重新 DISCOVER

---

## ⚠️ 待优化：主机-从机双向发现协议

### 问题描述
当前协议只能让主机认识从机，但从机不认识主机。当有多个从机时，从机只知道主机 MAC，不知道主机叫什么名字。

### 当前架构
| 方向 | 现状 | 问题 |
|------|------|------|
| 主机认识从机 | 从机发 ANNOUNCE 带 gpio_count 和 has_servo | ✅ 已解决 |
| 从机认识主机 | 从机只保存主机 MAC | ❌ 不知道主机叫什么 |

### 建议方案
新增 `IOT_CMD_MASTER_ANNOUNCE` 命令（0x40），主机定期广播：
```c
typedef struct {
    uint8_t command;           // 0x40 = IOT_CMD_MASTER_ANNOUNCE
    char master_name[32];      // "小智主机"
} iot_master_announce_packet_t;
```

### 优先级
**中等** - 目前从机只需知道主机 MAC 就能通信，名字是给人看的

### 其他待优化
1. 从机保存主机 MAC 到 NVS，启动后自动连接（无需每次 DISCOVER）
2. 从机掉线重连机制

---

## 2026-03-25 15:30（第一次会话）

### 项目背景
银发智护 - 三层楼智能家居系统，从机代码重构（药盒）

### 新增文件
- `xiaozhi_slave_yaohe/main/APP/Inc/esp_now_receive.h` - ESP-NOW 接收任务头文件
- `xiaozhi_slave_yaohe/main/APP/Inc/esp_now_heartbeat.h` - 心跳任务头文件
- `xiaozhi_slave_yaohe/main/APP/Inc/iot_control_task.h` - 设备控制任务头文件
- `xiaozhi_slave_yaohe/main/APP/Src/esp_now_receive.c` - ESP-NOW 接收 + 命令解析
- `xiaozhi_slave_yaohe/main/APP/Src/esp_now_heartbeat.c` - 心跳包发送任务
- `xiaozhi_slave_yaohe/main/APP/Src/iot_control_task.c` - Flag-driven 设备刷新任务（含调试按钮）

### 修改文件
- `xiaozhi_slave_yaohe/main/iot_slave_main.c` - 从 ~500 行精简到 ~70 行，只做初始化
- `xiaozhi_slave_yaohe/main/CMakeLists.txt` - 添加新的源文件

### 架构变更
- 新增 3 个 FreeRTOS 任务：
  - `esp_now_recv` - ESP-NOW 接收回调 + 命令解析
  - `esp_now_heartbeat` - 心跳任务（30s 间隔）
  - `iot_control_task` - 设备标志位刷新任务（100ms 刷新 GPIO）
- 采用 **Flag-driven 架构**：ESP-NOW 任务更新标志位，设备控制任务读取标志位刷新 GPIO
- 调试按钮集成到 `iot_control_task.c`，通过 `#define DEBUG_MODE` 控制开关
- 新增广播命令：`IOT_CMD_BROADCAST_ALL_OFF (0x30)`、`IOT_CMD_BROADCAST_ALL_ON (0x31)`

### 任务设计详情
| 任务名 | 优先级 | 栈大小 | 通信机制 | 同步机制 |
|--------|--------|--------|----------|----------|
| esp_now_recv | 5 | 2048 | 共享变量(g_device_flags) | 无锁(单核-safe) |
| iot_ctrl | 3 | 3072 | 无 | 无 |
| heartbeat | 4 | 2048 | 无 | 无 |
| debug_btn | 5 | 4096 | 无 | 无 |

### 设计理由
- **优先级 5 > 4 > 3**：ESP-NOW 回调需要最快响应，设备刷新最慢
- **共享变量 vs 队列**：数据量小（light_state + box_state < 10字节），无需队列开销
- **无互斥锁**：ESP-NOW 回调在 WiFi Task 中执行，100ms 刷新足够慢，volatile 保证可见性
- **Flag-driven 优势**：简化协议，接收任务只管更新标志，刷新任务自主轮询
