# AI 工作日志 (ai_working_log.md)

本文档记录所有代码修改历史，确保项目可追溯。

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
