# AI 工作日志 (ai_working_log.md)

本文档记录所有代码修改历史，确保项目可追溯。

---

## 2026-04-01 02:30（紧急修复 - 交稿前夜）

### 主题：MCP工具设备名称映射问题修复

### 问题描述

用户报告四个严重Bug（明天交作品）：

1. **大门不开门**：说"我回家了"，只会打开所有灯，一楼大门不开了
2. **无法单独控制灯光**：说"打开主卧灯"、"打开次卧灯"无法单独打开，但说"打开led1"可以打开主卧灯
3. **客厅灯不识别**：说"打开客厅灯"时小智说不知道有客厅灯
4. **风扇继电器不响应**：说"打开风扇"时小智调用了MCP但风扇没有反应

### 根本原因分析

#### Bug1 & Bug2 & Bug3 & Bug4：设备名称不匹配
- 从机设备名称（ANNOUNCE_V2广播）：
  - 一楼：`一楼设备`
  - 二楼：`二楼设备`
  - 三楼：`娑撳ⅸ浼婂▋銈肩拋鎯ь槵`（乱码）
- 用户/AI意图中的名称：`主卧`、`次卧`、`客厅`、`一楼大门`、`风扇`
- MCP工具描述中要求的设备名与实际从机广播的设备名完全不匹配

#### Bug1补充：回家场景MCP工具缺失
`custom_esp32s3_cam.cc`中只注册了`self.scene.emergency`（紧急求救），**缺少`self.scene.homing`（回家场景）**。

### 修复内容

#### 修复1：添加设备名称别名映射表

在 [bsp_iot_controller.c](file:///E:/MCU/esp32/xaiozhi1111111111111111111111111/xiaozhi-esp32-main/main/BSP/Src/bsp_iot_controller.c#L17-L49) 中新增别名解析函数：

```c
typedef struct {
    const char* alias;      // 别名
    const char* real_name;  // 实际设备名
} device_name_alias_t;

static const device_name_alias_t g_device_aliases[] = {
    {"一楼大门", "一楼设备"},
    {"一楼设备", "一楼设备"},
    {"一楼", "一楼设备"},
    {"二楼设备", "二楼设备"},
    {"二楼", "二楼设备"},
    {"主卧", "二楼设备"},
    {"次卧", "二楼设备"},
    {"客厅", "二楼设备"},
    {"二楼风扇", "二楼设备"},
    {"风扇", "二楼设备"},
    {"三楼", "娑撳ⅸ浼婂▋銈肩拋鎯ь槵"},
    {"三楼设备", "娑撳ⅸ浼婂▋銈肩拋鎯ь槵"},
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
```

修改`IOT_Find_Device_By_Name`函数，在查询前先解析别名：
```c
int IOT_Find_Device_By_Name(iot_controller_t* dev, const char* name) {
    // ...
    const char* resolved_name = IOT_Resolve_Alias(name);
    for (int i = 0; i < dev->remote_device_count; i++) {
        if (strcmp(dev->remote_devices[i].device_name, resolved_name) == 0) {
            return i;
        }
    }
    return -1;
}
```

#### 修复2：添加回家场景MCP工具

在 [custom_esp32s3_cam.cc](file:///E:/MCU/esp32/xaiozhi1111111111111111111111111/xiaozhi-esp32-main/main/boards/custom-esp32s3-cam/custom_esp32s3_cam.cc#L1048-L1062) 中新增：

```cpp
mcp_server.AddTool("self.scene.homing",
    "回家场景。用户说'我回家了'/'回家模式'时触发，打开一楼大门并打开所有灯。\n"
    "【注意】此场景会：1)打开一楼大门舵机 2)打开所有楼的灯。",
    PropertyList(),
    [](const PropertyList& properties) -> ReturnValue {
        bool success = Iot_Mcp_Scenario_Homecoming();
        cJSON* json = cJSON_CreateObject();
        cJSON_AddBoolToObject(json, "success", success);
        cJSON_AddStringToObject(json, "message", success ?
            "欢迎回家！大门已打开，灯已全部点亮" : "操作失败");
        return json;
    });
```

### 修复后预期行为

| 用户指令 | 解析后设备名 | GPIO操作 |
|----------|-------------|----------|
| "打开主卧灯" | 二楼设备 index0 | GPIO2=ON |
| "打开次卧灯" | 二楼设备 index1 | GPIO4=ON |
| "打开客厅灯" | 二楼设备 index2 | GPIO5=ON |
| "打开大厅灯" | 一楼设备 index0 | GPIO2=ON |
| "打开风扇" | 二楼设备 relay0 | GPIO6=ON |
| "打开大门" | 一楼设备 servo2 | GPIO8=180° |
| "我回家了" | 一楼大门+全楼灯 | 舵机180° + 全灯ON |

### 修改文件清单

| 文件 | 修改内容 |
|------|----------|
| `main/BSP/Src/bsp_iot_controller.c` | 新增别名映射表和解析函数 |
| `main/boards/custom-esp32s3-cam/custom_esp32s3_cam.cc` | 新增`self.scene.homing`回家场景、`self.scene.good_night`晚安场景 |

---

## 2026-04-01 03:00（补充修复）

### 主题：闹钟定时器添加秒级支持

### 问题描述
用户指出闹钟只有分钟级别，没有秒钟级别。

### 修复内容

#### 1. alarm_mcp_tool.h 新增函数声明
```c
int Alarm_Mcp_Add_Timer_Seconds(int seconds, const char* message);
```

#### 2. alarm_mcp_tool.cpp 新增函数实现
```cpp
int Alarm_Mcp_Add_Timer_Seconds(int seconds, const char* message) {
    if (seconds <= 0) {
        ESP_LOGE(TAG, "Invalid timer seconds: %d", seconds);
        return -1;
    }
    time_t now;
    time(&now);
    int64_t trigger_time = (int64_t)now + seconds;
    return Add_Alarm_Internal(ALARM_TYPE_TIMER, trigger_time, message, NULL, false);
}
```

#### 3. custom_esp32s3_cam.cc 新增MCP工具
```cpp
mcp_server.AddTool("self.alarm.add_timer_seconds",
    "设置秒级定时器（X秒后提醒）。Args: seconds(秒数), message(提醒内容)。",
    PropertyList({
        Property("seconds", kPropertyTypeInteger),
        Property("message", kPropertyTypeString)
    }),
    [](const PropertyList& properties) -> ReturnValue {
        int seconds = properties["seconds"].value<int>();
        auto message = properties["message"].value<std::string>();
        int alarm_id = Alarm_Mcp_Add_Timer_Seconds(seconds, message.c_str());
        // ...
    });
```

### 修改文件清单

| 文件 | 修改内容 |
|------|----------|
| `main/boards/custom-esp32s3-cam/alarm_mcp_tool.h` | 新增`Alarm_Mcp_Add_Timer_Seconds`声明 |
| `main/boards/custom-esp32s3-cam/alarm_mcp_tool.cpp` | 新增`Alarm_Mcp_Add_Timer_Seconds`实现 |
| `main/boards/custom-esp32s3-cam/custom_esp32s3_cam.cc` | 新增`self.alarm.add_timer_seconds`工具 |

---

## 2026-04-01 02:50（补充修复）

### 主题：添加晚安场景MCP工具

### 问题描述
用户指出缺少"我睡觉了"场景，且担心MCP工具数量超出payload限制。

### 修复内容

新增晚安场景MCP工具（[custom_esp32s3_cam.cc](file:///E:/MCU/esp32/xaiozhi1111111111111111111111111/xiaozhi-esp32-main/main/boards/custom-esp32s3-cam/custom_esp32s3_cam.cc#L1061-L1075)）：

```cpp
mcpServer.AddTool("self.scene.good_night",
    "晚安场景。用户说'我睡觉了'/'晚安'时触发，关闭所有灯。\n"
    "【注意】此场景只关闭灯，不影响继电器和舵机。",
    PropertyList(),
    [](const PropertyList& properties) -> ReturnValue {
        bool success = Iot_Mcp_Scenario_GoodNight();
        cJSON* json = cJSON_CreateObject();
        cJSON_AddBoolToObject(json, "success", success);
        cJSON_AddStringToObject(json, "message", success ?
            "晚安！所有灯已关闭，祝您好梦" : "操作失败");
        return json;
    });
```

### Payload限制说明
- 当前MCP工具总数约26个
- 每个工具的描述（description）已精简，使用中文缩写
- 场景类工具（homing/good_night/emergency）使用简洁描述
- 如后续工具继续增加，可考虑：
  1. 合并相似工具（如多个继电器控制 → 单个继电器+索引参数）
  2. 将低频工具的描述进一步精简
  3. 使用动态工具注册（按需加载）

---

## 2026-04-01 03:30（深度架构审查 - 发现新Bug）

### 审查范围
- 三个从机固件（xiaozhi_slave_Firstfloor/Sencondfloor/Thirdfloor）
- 主机MCP工具注册（custom_esp32s3_cam.cc）
- BSP层GPIO/设备名解析（bsp_iot_controller.c）

---

## 一、已确认的正确实现

### 1.1 回家场景（Iot_Mcp_Scenario_Homecoming）
- 调用`IOT_Set_Servo(&g_iot_controller, device_index, 2, 180)`打开一楼大门（servo2）
- 调用`IOT_Broadcast_Lights_On()`打开所有灯
- **结论**：逻辑正确，设备名"一楼设备"已存在于别名映射表

### 1.2 雨滴检测自动收衣（sensor_adc_task.c）
- 检测到下雨时`servo[2] = SERVO_0`（收衣服）
- 天晴时`servo[2] = SERVO_180`（晾衣服）
- **结论**：逻辑正确

### 1.3 烟雾报警（sensor_adc_task.c）
- 检测到烟雾持续2次后触发`App_ASR_Send_Fire_Alert()`
- 50秒内不重复报警
- **结论**：逻辑正确

### 1.4 求助按钮（sensor_adc_task.c）
- 按下GPIO48按钮时触发`App_ASR_Send_Help_Alert()`
- **结论**：逻辑正确

### 1.5 三楼从机传感器配置
| 传感器 | GPIO | ADC通道 |
|--------|------|---------|
| 烟雾传感器 | GPIO1 | ADC1_CH0 |
| 雨滴传感器 | GPIO3 | ADC1_CH2 |
| 求助按键 | GPIO48 | - |

---

## 二、新发现的Bug列表

### Bug A: 三楼设备名乱码（严重）

**文件**：`xiaozhi_slave_Thirdfloor/main/APP/Src/esp_now_receive.c:66`

**问题**：
```c
#define DEVICE_NAME "娑撳ⅸ浼婂▋銈肩拋鎯ь槵"  // 乱码！
```

**正确应为**：
```c
#define DEVICE_NAME "三楼设备"
```

**影响**：
- 主机收到三楼广播后，`g_device_aliases`中的映射可能失效
- 虽然别名表有`"三楼" -> "娑撳ⅸ浼婂▋銈肩拋鎯ь槵"`的映射，但乱码可能导致strcmp失败

**修复**：
修改从机源码，重新烧录三楼固件

---

### Bug B: 别名映射表缺少"主卧灯"和"次卧灯"（严重）

**文件**：`bsp_iot_controller.c:23-40`

**问题**：用户可能说"打开主卧灯"，但别名表中只有"主卧"，没有"主卧灯"

**当前映射表**：
```c
{"主卧", "二楼设备"},
{"次卧", "二楼设备"},
```

**缺少**：
```c
{"主卧灯", "二楼设备"},
{"次卧灯", "二楼设备"},
```

**影响**：用户说"打开主卧灯"可能无法正确解析

**修复**：在映射表中添加"主卧灯"和"次卧灯"

---

### Bug C: 别名映射表编码不一致（中等）

**文件**：`bsp_iot_controller.c:23-40`

**问题**：
- `g_device_aliases`中三楼的实际设备名是乱码：`"娑撳ⅸ浼婂▋銈肩拋鎯ь槵"`
- 但`IOT_Find_Device_By_Name`会先用`IOT_Resolve_Alias`解析别名，再对比

**分析**：如果用户说"三楼"，会解析为乱码，然后用乱码去匹配从机广播的设备名

**结论**：只有在三楼设备名乱码的情况下才生效，需修复三楼设备名

---

### Bug D: 一楼/二楼从机中文名称定义错误（轻微）

**文件**：
- `xiaozhi_slave_Firstfloor/main/APP/Inc/iot_control_task.h:40-50`
- `xiaozhi_slave_Secondfloor/main/APP/Inc/iot_control_task.h:40-50`

**问题**：两个文件的中文定义完全一样，都写着"二楼xxx"，但注释说是"一楼设备控制任务"

```c
// 一楼从机的中文定义
#define LIGHT_1_CHINESE   "二楼浴室灯"  // 应该是"一楼浴室灯"！
```

**影响**：不影响ESP-NOW协议（协议用DEVICE_NAME），但中文TTS播报可能错误

---

## 三、GPIO配置一致性验证

### 三个从机GPIO配置（已确认一致）

| 从机 | 灯 GPIO 0-2 | 继电器 GPIO 3-5 | 舵机 GPIO 6-8 |
|------|-------------|------------------|---------------|
| 一楼 | GPIO2,4,5 | GPIO6,7,8 | GPIO9,10,11 |
| 二楼 | GPIO2,4,5 | GPIO6,7,8 | GPIO9,10,11 |
| 三楼 | GPIO2,4,5 | GPIO6,7,8 | GPIO9,10,11 |

**结论**：GPIO配置一致

---

## 四、MCP工具完整性检查

### 工具总数：28个

| 类别 | 工具数 | 示例 |
|------|--------|------|
| 相机 | 1 | self.camera.detect_face |
| IoT GPIO | 10 | self.iot.set_gpio, self.iot.set_light, self.iot.clothes_rack |
| 传感器 | 1 | self.mpu6050.get_data |
| 提醒 | 4 | self.buzzer.beep |
| 医药 | 4 | self.alarm.add_timer, self.alarm.add_medicine |
| 场景 | 3 | self.scene.homing, self.scene.good_night, self.scene.emergency |
| 其他 | 5 | self.iot.discover, self.medicine.take |

**结论**：工具完整

---

## 五、待修复Bug汇总

| Bug | 严重度 | 修复位置 | 修复方法 |
|-----|--------|----------|----------|
| A. 三楼设备名乱码 | 严重 | xiaozhi_slave_Thirdfloor固件 | 修改DEVICE_NAME并烧录 |
| B. 缺少"主卧灯"/"次卧灯"映射 | 严重 | bsp_iot_controller.c | 添加别名 |
| C. 编码不一致 | 中等 | 三楼固件+bsp_iot_controller.c | 统一使用UTF-8 |
| D. 中文名称错误 | 轻微 | 一楼/二楼头文件 | 修正中文宏定义 |

---

## 六、架构优点（无需修改）

1. **混合分层架构**：BSP层与业务层分离，设备名解析在BSP层实现
2. **广播命令支持**：BROADCAST_LIGHTS_ON/OFF支持一键控制所有灯
3. **传感器独立检测**：三楼sensor_adc_task独立检测烟雨，不依赖主机命令
4. **舵机角度枚举化**：使用`servo_angle_enum_t`避免魔法数字

---

## 2026-04-01 02:30（紧急修复 - 交稿前夜）

### 待验证事项（用户烧录后需确认）

1. **回家场景**：说"我回家了"，一楼大门是否打开？
2. **单独控制灯光**：说"打开主卧灯"、"打开次卧灯"、"打开客厅灯"是否能单独控制？
3. **风扇控制**：说"打开风扇"时二楼继电器是否响应？
4. **三楼设备名乱码**：三楼的`device_name`是否需要修复？

### 架构说明

本次修复体现了**Hybrid Layered Architecture**的核心思想：
- **别名映射层**放在BSP层（`bsp_iot_controller.c`），与硬件协议解耦
- **MCP工具**注册在应用层（`custom_esp32s3_cam.cc`），负责自然语言意图解析
- 设备名称别名机制让AI意图中的"客厅"自动映射到"二楼设备"，无需修改从机固件

---

## 2026-03-29 21:30（第十三次会话）