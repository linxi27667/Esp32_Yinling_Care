# AI 工作日志 (ai_working_log.md)

本文档记录所有代码修改历史，确保项目可追溯。

---

## 2026-03-26 21:00（第九次会话）

### 主题：中文设备名称绑定 + 意图识别问题分析

### 完成内容

#### 1. 中文设备名称绑定（用于小智意图识别）

**修改文件**：
- `xiaozhi_slave_Firstfloor/main/APP/Inc/iot_control_task.h`
- `xiaozhi_slave_Secondfloor/main/APP/Inc/iot_control_task.h`
- `xiaozhi_slave_Thirdfloor/main/APP/Inc/iot_control_task.h`

**一楼设备中文名称**：
```c
#define LIGHT_1_CHINESE   "浴室灯"
#define LIGHT_2_CHINESE   "客厅灯"
#define LIGHT_3_CHINESE   "卧室灯"

#define RELAY_1_CHINESE   "电视电源"
#define RELAY_2_CHINESE   "空调电源"
#define RELAY_3_CHINESE   "热水器电源"

#define SERVO_1_CHINESE   "窗户"
#define SERVO_2_CHINESE   "窗帘"
#define SERVO_3_CHINESE   "门锁"
```

**二楼/三楼**：类似定义，添加 `二楼`/`三楼` 前缀。

---

#### 2. 意图识别问题（待解决）

**问题**："关闭所有灯" 会触发 "关闭所有设备"（包括继电器和舵机）

**原因**：小智的意图识别配置问题，需要在小智的意图配置文件中区分：
- `"关灯"` → 只关灯
- `"关闭所有"` / `"全关"` → 关闭所有设备

**待办**：需要用户提供小智意图配置文件位置进行修改。

---

#### 3. AudioCodec 卡死问题（暂不影响演示）

**日志**：`I (95489) AudioCodec: Set output enable to false`

**分析**：这是音频电源管理逻辑，当音频输出超过 `AUDIO_POWER_TIMEOUT_MS` 无活动时自动关闭输出。

**状态**：暂不影响演示，待后续排查。

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
