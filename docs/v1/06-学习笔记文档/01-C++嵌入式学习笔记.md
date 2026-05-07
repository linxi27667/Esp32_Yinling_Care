# C++ 嵌入式学习笔记

> 记录嵌入式开发中C++相关的知识点，特别是C与C++混合编程的要点。

---

## 一、C与C++混合编程

### 1.1 extern "C" 的作用

**问题场景**：C++代码调用C语言编写的函数时，链接器报错 `undefined reference`

**原因**：
- C++支持函数重载，编译器会对函数名进行"名称修饰"（Name Mangling）
- C语言没有名称修饰，函数名保持原样
- 链接时，C++编译器找的是修饰后的名字，找不到C函数

**解决方法**：
```c
// 在C头文件中添加 extern "C" 包装
#ifdef __cplusplus
extern "C" {
#endif

// C函数声明
void App_MPU6050_Init(i2c_master_bus_handle_t i2c_bus);
void App_Buzzer_Init(gpio_num_t gpio_pin, uint8_t active_level);

#ifdef __cplusplus
}
#endif
```

**记忆口诀**：
> C++调C，头文件加 `extern "C"`

---

### 1.2 名称修饰对比

```cpp
// C编译器看到的函数名
App_MPU6050_Init

// C++编译器看到的函数名（被修饰了）
_Z18App_MPU6050_InitP19i2c_master_bus_t
```

---

### 1.3 实际项目中的应用

```
项目结构：
├── BSP/Src/bsp_mpu6050.c      # C语言编写
├── BSP/Inc/bsp_mpu6050.h      # C头文件（需要extern "C"）
├── APP/Src/app_mpu6050.c      # C语言编写
├── APP/Inc/app_mpu6050.h      # C头文件（需要extern "C"）
└── boards/custom-esp32s3-cam/
    └── custom_esp32s3_cam.cc  # C++调用C函数
```

---

## 二、C++基础概念

### 2.1 类与对象

```cpp
// 类的定义
class Motor {
private:
    int speed;           // 私有成员变量
    
public:
    // 构造函数
    Motor() : speed(0) {}
    
    // 公有成员函数
    void setSpeed(int s) { 
        speed = s; 
    }
    
    int getSpeed() { 
        return speed; 
    }
};

// 创建对象
Motor myMotor;
myMotor.setSpeed(100);
```

---

### 2.2 继承

```cpp
// 基类
class Sensor {
public:
    virtual float read() = 0;  // 纯虚函数
    virtual ~Sensor() {}
};

// 派生类
class TempSensor : public Sensor {
public:
    float read() override {
        return 25.5f;
    }
};
```

---

### 2.3 单例模式

小智项目中的 `McpServer` 就是单例模式：

```cpp
class McpServer {
private:
    static McpServer* instance;
    
    // 私有构造函数，防止外部创建
    McpServer() {}
    
public:
    // 获取唯一实例
    static McpServer& GetInstance() {
        if (!instance) {
            instance = new McpServer();
        }
        return *instance;
    }
};

// 使用方式
auto& mcp_server = McpServer::GetInstance();
```

---

### 2.4 Lambda表达式（回调函数）

```cpp
// 传统函数指针
void OnButtonClick() {
    printf("Button pressed!");
}
button.OnClick(OnButtonClick);

// Lambda表达式（更简洁）
button.OnClick([]() {
    printf("Button pressed!");
});

// 带捕获的Lambda
int count = 0;
button.OnClick([&count]() {
    count++;
    printf("Pressed %d times", count);
});
```

---

## 三、MCP工具注册

### 3.1 无参数工具

```cpp
mcp_server.AddTool(
    "self.mpu6050.get_data",
    "Get motion data from MPU6050 sensor.\n"
    "Returns accelerometer, gyroscope, and temperature data.",
    PropertyList(),  // 无参数
    [](const PropertyList& properties) -> ReturnValue {
        cJSON* json = cJSON_CreateObject();
        cJSON_AddNumberToObject(json, "temperature", my_mpu.temp);
        return json;
    }
);
```

---

### 3.2 带参数工具

```cpp
mcp_server.AddTool(
    "self.buzzer.beep",
    "Control buzzer to make sounds.\n"
    "Args:\n"
    "  times: Number of beeps\n"
    "  duration_ms: Duration in milliseconds",
    PropertyList({
        Property("times", kPropertyTypeInteger),
        Property("duration_ms", kPropertyTypeInteger)
    }),
    [](const PropertyList& properties) -> ReturnValue {
        int times = properties["times"].value<int>();
        int duration = properties["duration_ms"].value<int>();
        
        App_Buzzer_Beep_Times(times, duration, duration);
        
        cJSON* json = cJSON_CreateObject();
        cJSON_AddStringToObject(json, "message", "蜂鸣器已响铃");
        return json;
    }
);
```

---

### 3.3 参数类型

| 类型常量 | C++类型 | 说明 |
|---------|--------|------|
| `kPropertyTypeInteger` | `int` | 整数 |
| `kPropertyTypeString` | `std::string` | 字符串 |
| `kPropertyTypeBoolean` | `bool` | 布尔值 |
| `kPropertyTypeNumber` | `double` | 浮点数 |

---

## 四、cJSON库使用

### 4.1 解析JSON

```c
#include "cJSON.h"

// 解析JSON字符串
const char* json_str = "{\"name\":\"xiaozhi\",\"age\":20}";
cJSON* root = cJSON_Parse(json_str);

// 获取值
cJSON* name = cJSON_GetObjectItem(root, "name");
printf("Name: %s\n", name->valuestring);  // "xiaozhi"

cJSON* age = cJSON_GetObjectItem(root, "age");
printf("Age: %d\n", age->valueint);  // 20

// 获取数组
cJSON* arr = cJSON_GetObjectItem(root, "skills");
int count = cJSON_GetArraySize(arr);
for (int i = 0; i < count; i++) {
    cJSON* item = cJSON_GetArrayItem(arr, i);
    printf("Skill: %s\n", item->valuestring);
}

// 释放内存（重要！）
cJSON_Delete(root);
```

---

### 4.2 构建JSON

```c
// 创建对象
cJSON* root = cJSON_CreateObject();
cJSON_AddStringToObject(root, "name", "xiaozhi");
cJSON_AddNumberToObject(root, "age", 20);

// 创建数组
cJSON* skills = cJSON_CreateArray();
cJSON_AddItemToArray(skills, cJSON_CreateString("C"));
cJSON_AddItemToArray(skills, cJSON_CreateString("ESP32"));
cJSON_AddItemToObject(root, "skills", skills);

// 创建嵌套对象
cJSON* device = cJSON_CreateObject();
cJSON_AddStringToObject(device, "chip", "ESP32-S3");
cJSON_AddItemToObject(root, "device", device);

// 转为字符串
char* json_str = cJSON_Print(root);
printf("%s\n", json_str);

// 释放内存
free(json_str);
cJSON_Delete(root);
```

---

### 4.3 常用API

| 函数 | 用途 |
|------|------|
| `cJSON_Parse()` | 解析JSON字符串 |
| `cJSON_Delete()` | 释放JSON对象 |
| `cJSON_GetObjectItem()` | 获取对象中的字段 |
| `cJSON_GetArraySize()` | 获取数组长度 |
| `cJSON_GetArrayItem()` | 获取数组元素 |
| `cJSON_CreateObject()` | 创建JSON对象 |
| `cJSON_CreateArray()` | 创建JSON数组 |
| `cJSON_AddStringToObject()` | 添加字符串字段 |
| `cJSON_AddNumberToObject()` | 添加数字字段 |
| `cJSON_AddBoolToObject()` | 添加布尔字段 |
| `cJSON_Print()` | 转为格式化字符串 |

---

## 五、常见错误与解决

### 5.1 undefined reference

**错误信息**：
```
undefined reference to `App_MPU6050_Init(i2c_master_bus_t*)'
```

**原因**：C++调用C函数时缺少 `extern "C"` 声明

**解决**：在C头文件中添加 `extern "C"` 包装

---

### 5.2 multiple definition

**错误信息**：
```
multiple definition of `my_mpu'
```

**原因**：变量在头文件中定义（而不是声明）

**解决**：
```c
// app_mpu6050.c 中定义（分配内存）
mpu_t my_mpu = {...};

// app_mpu6050.h 中声明（告诉编译器它存在）
extern mpu_t my_mpu;
```

---

## 六、学习资源

### 推荐书籍
- 《C Primer Plus》- C语言入门
- 《Effective C++》- C++进阶
- 《嵌入式C语言自我修养》- 嵌入式专用

### 在线资源
- ESP-IDF官方文档
- 小智开源项目源码
- cJSON GitHub仓库

---

## 七、每日学习记录

### 2024-03-17
- [x] 理解 `extern "C"` 的作用
- [x] 学习MCP工具注册流程
- [x] 掌握cJSON基本API

### 待学习
- [ ] C++ RAII资源管理
- [ ] 智能指针在嵌入式中的应用
- [ ] FreeRTOS任务调度原理

---

*持续更新中...*
