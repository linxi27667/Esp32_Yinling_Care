# ESP32-P4 LVGL + MQTT 从机控制集成方案

## 一、现状分析

### 当前架构（ESP32-S3 主机）

```
┌─────────────────────────────────────────────┐
│  ESP32-S3 主机 (custom-esp32s3-cam)         │
│                                             │
│  MCP Server ← AI 云端                       │
│     ↓ 工具调用                              │
│  iot_mcp_tool.cpp (MCP→App_IOT_* 桥接)      │
│     ↓                                      │
│  iot_controller.c (App_IOT_* 应用层)        │
│     ↓                                      │
│  bsp_iot_controller.c (BSP IOT 驱动层)      │
│     ↓ MQTT                                 │
│  从机设备 (一楼/二楼/三楼)                   │
└─────────────────────────────────────────────┘
```

### 解耦设计要点

`iot_controller_t` 结构体已通过函数指针实现硬件解耦：

```c
typedef struct iot_controller_dev {
    void (*Gpio_Init)(...);      // GPIO 初始化
    void (*Gpio_Write)(...);     // GPIO 写入
    int  (*Gpio_Read)(...);      // GPIO 读取
    bool (*Mqtt_Init)(void);     // MQTT 初始化
    bool (*Mqtt_Publish)(...);   // MQTT 发布
    void (*Delay_Ms)(uint32_t);  // 延时
    void (*Log_Info)(...);       // 日志
    void (*Log_Error)(...);      // 错误日志
    // ... 设备列表
} iot_controller_t;
```

P4 只需实现同样的接口即可复用全部 IoT 控制逻辑。

---

## 二、P4 集成步骤

### 第1步：复制 IoT 控制层

将以下文件加入 P4 项目编译：

```
shared/mqtt_iot_protocol.h      ← 共享协议（无需修改）
main/APP/Inc/iot_controller.h   ← APP 层头文件
main/APP/Src/iot_controller.c   ← APP 层实现（适配 P4 的 GPIO/MQTT）
main/BSP/Inc/bsp_iot_controller.h
main/BSP/Src/bsp_iot_controller.c
```

### 第2步：适配 P4 硬件接口

在 P4 的 `iot_controller.c` 中，替换 S3 的 GPIO 引脚：

```c
// S3 原来的引脚
#define LOCAL_GPIO_1_PIN    GPIO_NUM_48
#define LOCAL_GPIO_2_PIN    GPIO_NUM_46

// P4 改为你的实际引脚
#define LOCAL_GPIO_1_PIN    GPIO_NUM_XX  // P4 引脚
#define LOCAL_GPIO_2_PIN    GPIO_NUM_XX  // P4 引脚
```

MQTT Broker 地址保持不变（同一个 Mosquitto）：

```c
esp_mqtt_client_config_t mqtt_cfg = {
    .broker.address.uri = "mqtt://192.168.1.100",  // 你的电脑IP
    .broker.address.port = 1883,
    .credentials.client_id = "xiaozhi_p4_master",  // 不同 client_id
};
```

### 第3步：LVGL UI 调用 IoT 接口

LVGL 按钮回调直接调用 App_IOT_* 函数：

```c
// 示例：LVGL 按钮 - 打开客厅灯
static void lvgl_btn_living_room_on_cb(lv_event_t* e) {
    App_IOT_Set_Light("二楼设备", 1, true);  // 二楼 index1=客厅灯
}

// 示例：LVGL 滑块 - 控制舵机角度
static void lvgl_slider_door_cb(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    int angle = lv_slider_get_value(slider);
    App_IOT_Set_Servo_ByIndex("一楼设备", 0, (uint8_t)angle);
}

// 示例：LVGL 开关 - 一键关灯
static void lvgl_switch_all_off_cb(lv_event_t* e) {
    App_IOT_Broadcast_Lights_Off();
}

// 示例：获取设备状态刷新 UI
static void lvgl_refresh_status(void) {
    char buffer[1024];
    App_IOT_Get_All_Status(buffer, sizeof(buffer));
    // 解析 JSON，更新 LVGL 控件状态
}
```

### 第4步：P4 初始化流程

```c
void app_main(void) {
    // 1. 初始化 WiFi
    wifi_init_sta();

    // 2. 初始化 LVGL 显示
    lvgl_display_init();

    // 3. 初始化 IoT 控制器（内部会连 MQTT）
    App_IOT_System_Init();

    // 4. 创建 LVGL UI（按钮、开关等）
    create_iot_control_ui();

    // 5. 创建状态刷新定时器
    lv_timer_create(refresh_device_status, 2000, NULL);
}
```

---

## 三、S3 和 P4 双主机协同

### 方案：共享 MQTT Broker

```
                ┌──────────────┐
                │  Mosquitto   │
                │  (电脑/Docker)│
                └──┬───┬───┬──┘
                   │   │   │
          ┌────────┘   │   └────────┐
          ▼            ▼            ▼
    ┌──────────┐ ┌──────────┐ ┌──────────┐
    │ ESP32-S3 │ │ ESP32-P4 │ │ 从机设备  │
    │ (主机1)  │ │ (主机2)  │ │ 一楼/二楼 │
    │ AI语音   │ │ LVGL触屏 │ │ /三楼     │
    └──────────┘ └──────────┘ └──────────┘
```

### 关键设计点

1. **不同 client_id**：S3 用 `xiaozhi_master`，P4 用 `xiaozhi_p4_master`
2. **共享命令通道**：两台主机都往 `xiaozhi/iot/cmd/{device_id}` 发命令
3. **共享响应通道**：两台主机都订阅 `xiaozhi/iot/resp/+`、`xiaozhi/iot/heartbeat/+`
4. **状态同步**：P4 通过 `App_IOT_Get_All_Status` 获取从机状态并刷新 LVGL 显示
5. **命令幂等**：SET 命令天然幂等，两台主机同时发相同命令无副作用

### 注意事项

- 两台主机各自维护自己的设备列表和心跳超时检测
- 从机的 RESPONSE 会同时发给两台主机
- 如果要区分命令来源，可在 reserved 字段中标记

---

## 四、MCP 工具对照表（P4 可选）

如果不跑 AI 语音，P4 可以跳过 MCP Server，直接用 LVGL UI 调用 App_IOT_* 函数：

| LVGL UI 操作 | 对应的 App_IOT_* 调用 |
|---|---|
| 灯开关 | `App_IOT_Set_Light(device, index, on/off)` |
| 继电器开关 | `App_IOT_Set_Relay(device, index, on/off)` |
| 舵机滑块 | `App_IOT_Set_Servo_ByIndex(device, index, angle)` |
| 一键全关 | `App_IOT_Broadcast_All_Off()` |
| 一键全开 | `App_IOT_Broadcast_All_On()` |
| 全部关灯 | `App_IOT_Broadcast_Lights_Off()` |
| 全部开灯 | `App_IOT_Broadcast_Lights_On()` |
| 回家场景 | `App_IOT_Scenario_Homecoming("一楼设备", "all")` |
| 离家场景 | `App_IOT_Scenario_LeaveHome("一楼设备")` |
| 晚安场景 | `App_IOT_Scenario_GoodNight("all")` |
| 设备状态 | `App_IOT_Get_All_Status(buffer, size)` |

如果 P4 也需要 AI 语音，则注册同样的 MCP 工具即可。

---

## 五、关键文件清单

| 文件 | 作用 | 是否需要修改 |
|---|---|---|
| `shared/mqtt_iot_protocol.h` | MQTT Topic + 命令码 | 否（直接复用） |
| `main/APP/Inc/iot_controller.h` | APP 层接口声明 | 否 |
| `main/APP/Src/iot_controller.c` | APP 层实现 | 是（适配 P4 GPIO/MQTT） |
| `main/BSP/Inc/bsp_iot_controller.h` | BSP 层接口声明 | 否 |
| `main/BSP/Src/bsp_iot_controller.c` | BSP 设备管理 | 否（直接复用） |
| `main/boards/p4_board/xxx.cc` | P4 板级初始化 | 新建 |
| `main/lvgl_ui/iot_control_ui.c` | LVGL UI 界面 | 新建 |
