# 项目架构原理解析 (ARCHITECTURE.md)

## 一、整体架构概览

```
┌─────────────────────────────────────────────────────────────────┐
│                        用户交互层                                │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐             │
│  │   语音输入   │  │   按键交互   │  │   屏幕显示   │             │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘             │
└─────────┼────────────────┼────────────────┼─────────────────────┘
          │                │                │
          ▼                ▼                ▼
┌─────────────────────────────────────────────────────────────────┐
│                      应用核心层 (Application)                    │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                    Application 单例                      │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐   │   │
│  │  │ 状态机   │ │ 音频管理 │ │ 对话管理 │ │ 网络管理 │   │   │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘   │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
          │
          ▼
┌─────────────────────────────────────────────────────────────────┐
│                      MCP协议层 (MCP Server)                      │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │              Model Context Protocol                      │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐   │   │
│  │  │ 工具注册 │ │ 消息处理 │ │ 结果返回 │ │ AI调用   │   │   │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘   │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
          │
          ▼
┌─────────────────────────────────────────────────────────────────┐
│                      开发板抽象层 (Board)                        │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │              Board 基类 + 开发板实现                      │   │
│  │  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐   │   │
│  │  │ AudioCodec│ │ Display  │ │ Camera   │ │ Led      │   │   │
│  │  └──────────┘ └──────────┘ └──────────┘ └──────────┘   │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
          │
          ▼
┌─────────────────────────────────────────────────────────────────┐
│                      硬件驱动层 (ESP-IDF)                        │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐           │
│  │ GPIO     │ │ I2S      │ │ I2C      │ │ Camera   │           │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘           │
└─────────────────────────────────────────────────────────────────┘
```

---

## 二、核心组件详解

### 2.1 Application - 应用核心

**文件**: `application.cc/h`

**职责**:
- 管理设备状态机
- 协调各子系统工作
- 处理用户交互事件

**状态机**:
```
┌─────────────┐     启动完成      ┌─────────────┐
│   Starting  │ ────────────────> │   Idle      │
└─────────────┘                   └──────┬──────┘
                                         │
                          按键/唤醒词     │
                                         ▼
                                  ┌─────────────┐
                                  │  Listening  │
                                  └──────┬──────┘
                                         │
                              语音识别完成│
                                         ▼
                                  ┌─────────────┐
                                  │  Speaking   │
                                  └─────────────┘
```

### 2.2 MCP Server - 工具协议服务器

**文件**: `mcp_server.cc/h`

**职责**:
- 实现 Model Context Protocol
- 注册和管理工具
- 处理 AI 的工具调用请求

**工具注册流程**:
```cpp
// 1. 定义工具
mcp_server->AddTool("self.camera.take_photo",
    "拍照并分析",
    PropertyList({Property("question", kPropertyTypeString)}),
    [camera](const PropertyList& props) -> ReturnValue {
        camera->Capture();
        return camera->Explain(props["question"].value<std::string>());
    });

// 2. AI 自动识别可用工具
// 3. 用户请求时，AI 决定调用哪个工具
// 4. 执行工具并返回结果
```

### 2.3 Board - 开发板抽象

**文件**: `boards/common/board.h`

**核心设计模式**: 依赖注入 + 工厂模式

```cpp
class Board {
public:
    static Board& GetInstance();  // 单例获取
    
    virtual AudioCodec* GetAudioCodec() = 0;  // 音频编解码器
    virtual Display* GetDisplay() = 0;        // 显示器
    virtual Camera* GetCamera() = 0;          // 摄像头
    virtual Led* GetLed() = 0;                // LED
    
    virtual void InitializeTools(McpServer* mcp);  // 初始化MCP工具
};

// 工厂宏 - 自动注册开发板
#define DECLARE_BOARD(BoardClass) \
    static Board* CreateBoard_##BoardClass() { \
        return new BoardClass(); \
    } \
    static bool registered_##BoardClass = BoardFactory::Register(CreateBoard_##BoardClass);
```

---

## 三、两层四文件架构

### 3.1 架构原则

```
┌─────────────────────────────────────────────────────────────────┐
│                          APP 层                                  │
│  【物理绑定层 - 唯一可以包含硬件头文件的地方】                    │
│                                                                  │
│  职责:                                                           │
│  1. 分配物理引脚                                                 │
│  2. 实例化 BSP 对象                                              │
│  3. 编写 HW_ 前缀的硬件操作函数                                  │
│  4. 将硬件函数注入到 BSP 对象的函数指针                          │
│                                                                  │
│  命名: 直接使用模块名 (如 iot_controller.h/c)                    │
│  禁止: 使用 app_ 前缀                                            │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ 依赖注入
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                          BSP 层                                  │
│  【纯逻辑层 - 绝对硬件无关】                                      │
│                                                                  │
│  职责:                                                           │
│  1. 定义设备对象结构体                                           │
│  2. 实现纯逻辑算法                                               │
│  3. 通过函数指针调用底层                                         │
│  4. 暴露极简的阻塞式/状态机 API                                  │
│                                                                  │
│  命名: 必须使用 bsp_ 前缀 (如 bsp_iot_controller.h/c)            │
│  禁止: 包含任何芯片头文件 (如 driver/gpio.h)                     │
└─────────────────────────────────────────────────────────────────┘
```

### 3.2 代码示例

**BSP 层头文件** (`bsp_iot_controller.h`):
```cpp
#ifndef __BSP_IOT_CONTROLLER_H__
#define __BSP_IOT_CONTROLLER_H__

#include <stdint.h>
#include <stdbool.h>

// 抽象 GPIO 泛型
typedef struct {
    void* port;      // 兼容所有平台
    uint16_t pin;    // 引脚号
} iot_gpio_t;

// 设备对象
typedef struct iot_dev {
    // 物理配置
    iot_gpio_t local_gpios[8];
    int gpio_count;
    
    // 状态
    uint8_t gpio_states[8];
    
    // 抽象操作方法 (函数指针)
    void (*Gpio_Write)(void* port, uint16_t pin, uint8_t level);
    int  (*Gpio_Read)(void* port, uint16_t pin);
    void (*Remote_Send)(uint8_t* data, uint16_t len);
} iot_controller_t;

// 对外 API
void IOT_Controller_Init(iot_controller_t* dev);
void IOT_Set_GPIO(iot_controller_t* dev, int index, uint8_t level);
int  IOT_Get_GPIO(iot_controller_t* dev, int index);
void IOT_Set_Remote_GPIO(iot_controller_t* dev, uint8_t device_id, uint8_t pin, uint8_t level);

#endif
```

**APP 层实现** (`iot_controller.c`):
```cpp
#include "main.h"  // 【唯一解禁区】包含硬件头文件
#include "bsp_iot_controller.h"

// ================= 1. 硬件底层函数 (HW_ 前缀) =================
static void HW_Gpio_Write(void* port, uint16_t pin, uint8_t level) {
    gpio_set_level((gpio_num_t)pin, level);
}

static int HW_Gpio_Read(void* port, uint16_t pin) {
    return gpio_get_level((gpio_num_t)pin);
}

static void HW_Remote_Send(uint8_t* data, uint16_t len) {
    // ESP-NOW 或 UART 发送
    esp_now_send(peer_addr, data, len);
}

// ================= 2. 实例化对象并拼装引脚 =================
iot_controller_t IOT_Controller = {
    .local_gpios = {
        {NULL, GPIO_NUM_3},  // GPIO 3
        {NULL, GPIO_NUM_4},  // GPIO 4
        // ...
    },
    .gpio_count = 8,
    .Gpio_Write = HW_Gpio_Write,
    .Gpio_Read = HW_Gpio_Read,
    .Remote_Send = HW_Remote_Send,
};

// ================= 3. 提供给上层的统一切入点 =================
void App_IOT_System_Init(void) {
    IOT_Controller_Init(&IOT_Controller);
}
```

---

## 四、数据流分析

### 4.1 语音控制流程

```
用户说话: "打开客厅的灯"
         │
         ▼
┌─────────────────┐
│   麦克风采集     │ I2S DMA
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│   音频预处理     │ AEC, NS, AGC
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│   语音识别       │ WebSocket → 服务器 ASR
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│   AI 理解意图    │ LLM 分析 + MCP 工具选择
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│   调用 MCP 工具  │ self.iot.set_gpio
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│   执行硬件操作   │ GPIO / ESP-NOW
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│   语音回复       │ TTS → I2S → 扬声器
└─────────────────┘
```

### 4.2 MCP 工具调用机制

```json
// 1. AI 请求调用工具
{
    "method": "tools/call",
    "params": {
        "name": "self.iot.set_gpio",
        "arguments": {
            "device": "living_room",
            "pin": 0,
            "value": true
        }
    }
}

// 2. MCP Server 执行工具
// 3. 返回结果
{
    "content": [{
        "type": "text",
        "text": "{\"success\":true,\"message\":\"客厅灯已打开\"}"
    }]
}
```

---

## 五、扩展开发指南

### 5.1 添加新的 MCP 工具

```cpp
// 在开发板的 InitializeTools 方法中添加
virtual void InitializeTools(McpServer* mcp_server) override {
    mcp_server->AddTool("self.my_tool",
        "工具描述，AI会根据这个描述决定是否调用",
        PropertyList({
            Property("param1", kPropertyTypeString),
            Property("param2", kPropertyTypeInteger, 0, 100)  // 带范围限制
        }),
        [this](const PropertyList& props) -> ReturnValue {
            // 实现逻辑
            return "执行结果";
        });
}
```

### 5.2 添加新的 BSP 模块

1. 在 `main/BSP/Inc/` 创建 `bsp_xxx.h`
2. 在 `main/BSP/Src/` 创建 `bsp_xxx.c`
3. 在 `main/APP/Inc/` 创建 `xxx.h`
4. 在 `main/APP/Src/` 创建 `xxx.c`
5. 在 CMakeLists.txt 添加源文件

### 5.3 添加新的开发板

1. 在 `main/boards/` 创建目录
2. 创建 `config.h` 定义引脚
3. 创建开发板主文件继承 `WifiBoard` 或 `Ml307Board`
4. 在 CMakeLists.txt 和 Kconfig.projbuild 添加配置

---

## 六、关键技术点

### 6.1 内存管理

- **PSRAM**: 摄像头帧缓冲、YOLO模型
- **IRAM**: 时间敏感代码
- **DRAM**: 普通变量

### 6.2 多任务架构

```
┌──────────────┐  ┌──────────────┐  ┌──────────────┐
│   Main Task  │  │  Audio Task  │  │ Network Task │
│   (Core 0)   │  │   (Core 1)   │  │   (Core 0)   │
└──────────────┘  └──────────────┘  └──────────────┘
```

### 6.3 通信协议

| 协议 | 用途 | 特点 |
|------|------|------|
| WebSocket | AI对话 | 全双工、低延迟 |
| HTTP | OTA升级 | 可靠传输 |
| ESP-NOW | 设备间通信 | 低延迟、无路由 |
| MQTT | 云端控制 | 发布订阅模式 |

---

*文档创建时间: 2025年*
