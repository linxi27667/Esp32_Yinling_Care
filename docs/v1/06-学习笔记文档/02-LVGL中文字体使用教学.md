# LVGL 中文字体使用教学 (ESP-IDF 5.53)

---

## 一、字体文件概览

| 类别 | 文件 | 大小 | 推荐用途 |
|------|------|------|----------|
| **标准中文字体** | `lv_font_chinese_12.c` ~ `38.c` | 2MB~14MB | 日常 UI 文字显示 |
| **大号 Regular** | `lv_font_han_sans_regular_26~32.c` | 13MB~17MB | 标题、大段文字 |
| **大号数字** | `lv_font_han_sans_bold_number_only_86/114.c` | 279K~468K | 仪表、时钟显示 |
| **头文件** | `lv_font_chinese.h` | 1KB | 字体声明统一入口 |

> **⚠️ 每个 .c 文件编译后会占用 ESP32 Flash 空间**，请根据实际可用 Flash 按需添加。

---

## 二、快速开始

### 步骤 1: 将字体文件添加到项目

将需要的 `.c` 文件复制到你的 ESP-IDF 项目组件目录，例如：

```
your_project/
├── components/
│   └── fonts/
│       ├── lv_font_chinese.h        ← 必须
│       ├── lv_font_chinese_16.c     ← 按需选择
│       ├── lv_font_chinese_24.c     ← 按需选择
│       └── CMakeLists.txt           ← 见下方
```

### 步骤 2: 编写组件 CMakeLists.txt

```c
// components/fonts/CMakeLists.txt
idf_component_register(
    SRCS
        lv_font_chinese_16.c     // 按需添加所需文件
        lv_font_chinese_24.c
    INCLUDE_DIRS "."
    REQUIRES lvgl
)
```

### 步骤 3: 在项目 CMakeLists.txt 中注册组件

```cmake
# CMakeLists.txt (项目根目录)
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS components)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(my_lvgl_project)
```

### 步骤 4: 在代码中使用

```c
#include "lvgl.h"
#include "lv_font_chinese.h"  // 引入字体声明

void app_main(void)
{
    // 初始化 LVGL（省略初始化代码）
    // ...

    // 创建一个标签并设置中文字体
    lv_obj_t *label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, "你好世界！Hello World!");
    lv_obj_set_style_text_font(label, &lv_font_chinese_16, 0);  // 16px 字体
    lv_obj_center(label);

    // 使用 24px 字体
    lv_obj_t *title = lv_label_create(lv_scr_act());
    lv_label_set_text(title, "设备状态：正常运行");
    lv_obj_set_style_text_font(title, &lv_font_chinese_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);
}
```

---

## 三、常用字号选择建议

| 场景 | 推荐字体 | 原因 |
|------|----------|------|
| 状态栏/小字提示 | `lv_font_chinese_12` / `14` | 节省 Flash，小场景够用 |
| **正文标签 (最常用)** | `lv_font_chinese_16` / `18` | 平衡清晰度和大小 |
| 标题/按钮文字 | `lv_font_chinese_20` / `24` | 视觉突出 |
| 仪表盘大数字 | `lv_font_han_sans_bold_number_only_86` | 仅数字，文件小 (279K) |
| 全屏时钟显示 | `lv_font_han_sans_bold_number_only_114` | 超大号数字 |

---

## 四、进阶用法

### 4.1 通过 menuconfig 动态选择字体

在 `sdkconfig` 中添加自定义配置，在编译时选择字体：

```c
// 在你的配置头文件中
#if CONFIG_USE_CHINESE_FONT_16
#define MY_FONT lv_font_chinese_16
#elif CONFIG_USE_CHINESE_FONT_24
#define MY_FONT lv_font_chinese_24
#endif
```

### 4.2 设置为全局默认字体

```c
// 在 LVGL 初始化后设置全局默认字体
lv_theme_t *theme = lv_theme_default_init(
    display,
    lv_palette_main(LV_PALETTE_BLUE),
    lv_palette_main(LV_PALETTE_RED),
    false,
    &lv_font_chinese_16  // 默认字体
);
```

### 4.3 混合使用多种字体

```c
// 主标题用大号字体
lv_obj_set_style_text_font(title_label, &lv_font_chinese_24, 0);

// 副标题用中号字体
lv_obj_set_style_text_font(subtitle_label, &lv_font_chinese_16, 0);

// 数字显示用大号数字字体
lv_obj_set_style_text_font(number_label, &lv_font_han_sans_bold_number_only_86, 0);
```

### 4.4 在 LVGL SquareLine Studio / GUI Guider 中使用

1. 将 `.c` 文件复制到 GUI Guider 项目的 `generated/font` 或 `custom/font` 目录
2. 在 GUI Guider 的字体管理器中导入外部字体
3. 或者在生成的代码中手动替换字体引用

---

## 五、Flash 空间估算

| 选择方案 | 包含字号 | 预估 Flash 占用 |
|----------|----------|----------------|
| 精简方案 | 16px 仅一个 | ~3MB |
| 标准方案 | 12, 16, 20, 24 | ~15MB |
| 完整方案 | 全部 14 个字号 | ~100MB+ |
| 数字专用 | 86px, 114px 数字 | ~750KB |

> **建议**: ESP32 一般 Flash 为 4MB~16MB，推荐使用 **精简方案** 或 **标准方案**。
> 如果 Flash 空间有限，仅选择 1~2 个最常用字号。

---

## 六、常见问题

### Q1: 显示乱码怎么办？

**原因分析**:
1. 字体未包含对应字符的 Unicode 映射
2. 编码格式不匹配（应为 UTF-8）
3. 字体文件损坏或未正确编译

**解决方法**:
```c
// 确保源文件保存为 UTF-8 编码
// 在 VSCode 中：右下角 → UTF-8 → Save with Encoding → UTF-8

// 确保 LVGL 启用 UTF-8 支持
// 在 lv_conf.h 中确认：
#define LV_TXT_ENC LV_TXT_ENC_UTF8
```

### Q2: 编译时报错 "undefined reference to lv_font_chinese_16"

**原因**: 字体 `.c` 文件未添加到编译源文件列表。

**解决**: 检查 `CMakeLists.txt` 的 `SRCS` 是否包含该文件。

### Q3: 编译成功但运行后字体显示为方块

**原因**: LVGL 找不到字体数据，可能是 `LV_FONT_CHINESE_16` 宏未定义或 LVGL 未初始化。

**解决**:
```c
// 1. 确保 LVGL 已初始化
lv_init();

// 2. 确认字体文件中的宏已启用
// 打开 lv_font_chinese_16.c，确保：
#define LV_FONT_CHINESE_16 1
```

### Q4: 需要特殊字符（如生僻字、日文假名）

当前字体覆盖 **~20,468 常用汉字**，如果遇到未覆盖的字符显示为方块：

- **方案 A**: 使用 `lv_font_conv` 工具从 TTF 重新生成，添加所需字符范围
- **方案 B**: 使用 LVGL 的 fallback 字体机制组合多个字体

---

## 七、字符覆盖范围

| 字符类型 | 覆盖情况 |
|----------|----------|
| ASCII (英文字母/数字/符号) | ✅ 完整 |
| 常用汉字 (GB2312 一级) | ✅ 完整 (~6763 字) |
| 扩展常用汉字 (GB2312 二级) | ✅ 大部分 (~13,700 字) |
| 中文标点 | ✅ 完整 |
| 日文假名/韩文 | ❌ 不保证 |
| 生僻字 | ❌ 不保证 |

---

## 八、完整示例代码

```c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_font_chinese.h"

static const char *TAG = "demo";

static void lv_demo_chinese(void)
{
    // 主标题
    lv_obj_t *title = lv_label_create(lv_scr_act());
    lv_label_set_text(title, "ESP32 中文显示示例");
    lv_obj_set_style_text_font(title, &lv_font_chinese_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    // 正文内容
    lv_obj_t *content = lv_label_create(lv_scr_act());
    lv_label_set_text(content,
        "这是一段中文测试文字。\n"
        "思源黑体，4bpp 灰度抗锯齿。\n"
        "当前字号: 16px");
    lv_obj_set_style_text_font(content, &lv_font_chinese_16, 0);
    lv_obj_align(content, LV_ALIGN_CENTER, 0, 30);

    // 英文混排测试
    lv_obj_t *mixed = lv_label_create(lv_scr_act());
    lv_label_set_text(mixed,
        "Hello 你好! 123 ABC abc\n"
        "温度: 25.6°C  湿度: 60%");
    lv_obj_set_style_text_font(mixed, &lv_font_chinese_16, 0);
    lv_obj_align(mixed, LV_ALIGN_BOTTOM_MID, 0, -30);
}

void app_main(void)
{
    ESP_LOGI(TAG, "初始化 LVGL...");

    lv_init();
    lv_port_disp_init();

    lv_demo_chinese();

    while (1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
```

---

*文档生成时间: 2026-04-18*
*字体来源: [isnowify/lvgl_chinese_fonts](https://github.com/isnowify/lvgl_chinese_fonts) - Adobe Han Sans (思源黑体)*
