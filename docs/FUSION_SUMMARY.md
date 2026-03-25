# 项目融合总结 (FUSION_SUMMARY.md)

## 一、融合概述

本文档记录了将原项目与小智AI开源项目进行融合的完整过程。

---

## 二、原项目结构

原项目采用**两层四文件架构**，严格遵循高内聚、低耦合原则：

```
main/
├── APP/                          # 应用层（物理绑定层）
│   ├── Inc/
│   │   ├── app_wifi.h
│   │   ├── app_video_server.h
│   │   ├── app_yolo.h
│   │   ├── app_uart.h
│   │   ├── app_audio.h
│   │   └── app_message.h
│   └── Src/
│       ├── app_wifi.c
│       ├── app_video_server.c
│       ├── app_yolo.cpp
│       ├── app_uart.c
│       ├── app_audio.c
│       └── app_message.c
│
├── BSP/                          # 板级支持包（纯逻辑层）
│   ├── Inc/
│   │   ├── bsp_wifi.h
│   │   ├── bsp_camera.h
│   │   ├── bsp_uart.h
│   │   └── bsp_audio.h
│   └── Src/
│       ├── bsp_wifi.c
│       ├── bsp_camera.c
│       ├── bsp_uart.c
│       └── bsp_audio.c
│
└── oldprojectmain.c              # 原项目入口
```

---

## 三、小智AI项目结构

小智AI采用**面向对象的开发板抽象架构**：

```
main/
├── boards/                       # 开发板配置目录
│   ├── common/                   # 公共组件
│   │   ├── camera.h              # 摄像头抽象接口
│   │   ├── esp32_camera.cc       # ESP32摄像头实现
│   │   ├── esp_video.cc          # 视频处理
│   │   ├── display.h             # 显示抽象接口
│   │   ├── oled_display.h/cc     # OLED显示实现
│   │   └── ...
│   │
│   ├── bread-compact-wifi-s3cam/ # 面包板开发板
│   ├── esp-box-3/                # ESP-BOX-3
│   └── [其他开发板...]
│
├── application.cc/h              # 应用核心
├── mcp_server.cc/h               # MCP协议服务器
└── protocols/                    # 通信协议
```

---

## 四、融合策略

### 4.1 核心思路

**不修改小智AI核心代码，通过创建自定义开发板实现功能整合**

### 4.2 创建的文件

| 文件 | 位置 | 作用 |
|------|------|------|
| `config.h` | boards/custom-esp32s3-cam/ | 引脚配置定义 |
| `custom_esp32s3_cam.cc` | boards/custom-esp32s3-cam/ | 开发板主文件 |
| `yolo_mcp_tool.h` | boards/custom-esp32s3-cam/ | YOLO工具头文件 |
| `yolo_mcp_tool.cpp` | boards/custom-esp32s3-cam/ | YOLO工具实现 |

### 4.3 修改的文件

| 文件 | 修改内容 |
|------|----------|
| `CMakeLists.txt` | 添加新开发板构建配置 |
| `Kconfig.projbuild` | 添加新开发板菜单选项 |
| `oldprojectmain.c` | 清理豆包相关注释 |

---

## 五、引脚配置

### 5.1 最终引脚分配

```
ESP32-S3-N8R8 引脚分配表
========================

摄像头接口 (OV5640)
------------------
D0  -> GPIO11      D4  -> GPIO12
D1  -> GPIO9       D5  -> GPIO18
D2  -> GPIO8       D6  -> GPIO17
D3  -> GPIO10      D7  -> GPIO16
XCLK -> GPIO15     PCLK -> GPIO13
VSYNC -> GPIO6     HREF -> GPIO7
SDA -> GPIO4       SCL -> GPIO5

音频接口 (I2S)
------------------
麦克风:
  WS  -> GPIO1
  SCK -> GPIO2
  DIN -> GPIO42

扬声器:
  DOUT -> GPIO39
  BCLK -> GPIO40
  LRCK -> GPIO41

显示接口 (I2C OLED SSD1306)
------------------
SDA -> GPIO38
SCL -> GPIO14    # 注意：从GPIO39改到GPIO14，避免冲突

其他
------------------
LED -> GPIO48
BOOT按键 -> GPIO0
```

### 5.2 引脚冲突解决

| 冲突 | 原配置 | 解决方案 |
|------|--------|----------|
| OLED SCL vs 扬声器 DOUT | 都在GPIO39 | OLED SCL改到GPIO14 |

---

## 六、功能整合

### 6.1 摄像头功能

- 使用小智AI的 `Esp32Camera` 类
- 配置为 RGB565 格式，QVGA 分辨率
- 支持水平镜像和垂直翻转

### 6.2 OLED显示

- 使用小智AI的 `OledDisplay` 类
- SSD1306 驱动，128x64 分辨率
- I2C 接口，地址 0x3C

### 6.3 YOLO人脸检测

- 作为 MCP 工具集成
- 工具名称: `self.camera.detect_face`
- 使用 ESP-DL 库的 HumanFaceDetect 模型

### 6.4 豆包功能替换

- 原豆包功能已完全移除
- 由小智AI的语音对话功能替代

---

## 七、构建配置

### 7.1 选择开发板

```bash
idf.py menuconfig
# Board Options -> Board Type -> Custom ESP32-S3 CAM
```

### 7.2 编译命令

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p COM端口 flash monitor
```

---

## 八、保留的原项目文件

以下文件保留但未纳入构建系统，供后续开发参考：

```
main/APP/     # 应用层代码
main/BSP/     # BSP层代码
```

如需使用这些模块，需要：
1. 在 CMakeLists.txt 中添加源文件
2. 根据小智AI的架构进行适配

---

## 九、后续扩展建议

1. **IoT控制** - 添加GPIO控制MCP工具
2. **传感器扩展** - 集成更多传感器驱动
3. **自定义唤醒词** - 训练专属唤醒词
4. **OTA升级** - 配置远程固件更新

---

*文档创建时间: 2025年*
*基于小智AI开源项目*
