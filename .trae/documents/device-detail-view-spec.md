# 独立设备控制界面设计规格书

## 1. 项目概述

### 1.1 目标
为"银龄智享"IoT控制面板开发一个功能完整的独立设备控制详情页，用户点击首页设备卡片后进入该页面，可查看设备完整状态、调节参数、执行操作控制。

### 1.2 设计原则
- **Apple Human Interface Guidelines**: 延续现有玻璃拟态、Bento Box布局、Spring动画风格
- **一致性**: 与现有Apple风格UI保持完全一致的视觉语言
- **可访问性**: 大字体、高对比度、清晰的操作反馈，适合老年用户
- **流畅性**: 页面切换使用Spring物理动画，操作反馈即时

---

## 2. 架构设计

### 2.1 路由结构
```
/                    → HomeView (现有首页，设备总览仪表盘)
/device/:deviceId    → DeviceDetailView (新增设备详情控制页)
```

### 2.2 页面切换动画
- **进入动画**: 从右侧滑入 + 淡入，使用 `cubic-bezier(0.34, 1.56, 0.64, 1)`
- **退出动画**: 向右滑出 + 淡出
- **导航栏**: 详情页顶部显示返回按钮 + 设备名称

### 2.3 数据流
- 复用现有的 `useDeviceStore` Pinia store
- 通过 `deviceId` 路由参数从 store 中获取目标设备数据
- WebSocket 连接在全局维持，详情页实时接收设备状态更新

---

## 3. 设备详情页布局设计

### 3.1 页面结构 (从上到下)

#### 3.1.1 导航栏 (Apple Glassmorphism)
- 左侧: 返回箭头按钮 (←) + "返回"文字
- 中间: 设备名称 (动态)
- 右侧: 在线状态指示器 (与 StatusBadge 一致)
- 样式: `apple-glass-strong`, 高度 52px

#### 3.1.2 设备信息头部卡片 (Hero Section)
- 设备图标/头像 (大尺寸，带渐变背景)
- 设备名称 (apple-headline)
- MAC地址 (apple-mono, 次要色)
- 在线状态 + 最后心跳时间
- 背景: Bento Card 样式，可添加微妙的设备类型渐变

#### 3.1.3 实时数据监控区 (Bento Grid)
- **GPIO状态总览**: 所有GPIO的实时值展示
  - 灯光: 开关状态 + 亮度指示
  - 继电器: 运行/停止状态
  - 舵机: 当前角度值
- **信号强度/连接质量** (如数据可用)
- **设备运行时长** (如数据可用)
- 布局: 2x2 或自适应网格

#### 3.1.4 控制面板区 (核心功能区)
- **灯光控制组**
  - 每个灯光独立的开关 + 亮度滑块 (0-100%)
  - 使用现有的 LightControl 组件样式
- **继电器控制组**
  - 每个继电器独立的开关
  - 使用现有的 RelayControl 组件样式
- **舵机控制组**
  - 每个舵机独立的角度滑块 (0-180°)
  - 使用现有的 ServoControl 组件样式
- 布局: 按类型分组，每组一个 Bento Card

#### 3.1.5 操作日志区
- 该设备的专属操作日志
- 使用现有的 LogPanel 样式，但过滤只显示当前设备相关日志
- 时间线形式展示

#### 3.1.6 快捷场景按钮区
- 针对该设备的快捷场景:
  - 全开 / 全关
  - 重置设备
- 按钮样式: Bento Card 小按钮

---

## 4. 交互设计

### 4.1 进入详情页
- 用户在首页点击任意 DeviceCard
- 触发路由跳转 `/device/:deviceId`
- 页面从右侧滑入，带有 Spring 动画

### 4.2 返回首页
- 点击顶部导航栏返回按钮
- 或手势右滑返回 (移动端)
- 页面向右滑出，恢复首页

### 4.3 实时更新
- 设备状态通过 WebSocket 实时推送
- 详情页中所有控制组件实时响应状态变化
- 操作反馈: 发送命令后显示 pending 状态，收到响应后更新

### 4.4 空状态处理
- 如果 deviceId 不存在或设备已离线，显示空状态页面
- 空状态包含: 插图、提示文字、返回按钮

---

## 5. 视觉设计规范

### 5.1 色彩
- 完全复用 `apple-theme.css` 中的 CSS 变量系统
- 设备类型渐变:
  - 主控设备: `linear-gradient(135deg, #0A84FF, #0071E3)`
  - 从机设备: `linear-gradient(135deg, #30D158, #0A84FF)`

### 5.2 字体
- 完全复用现有字体栈: `var(--apple-font-stack)`
- 层级:
  - 页面标题: `apple-headline` (28px)
  - 区域标题: `apple-title` (21px)
  - 正文: `apple-body` (17px)
  - 辅助: `apple-caption` (13px)

### 5.3 间距与圆角
- 完全复用现有设计系统变量
- 卡片内边距: `var(--apple-space-lg)` (24px)
- 卡片圆角: `var(--apple-radius-2xl)` (24px)
- 模块间距: `var(--apple-space-lg)` (24px)

### 5.4 动画
- 页面切换: Spring 物理动画
- 元素入场: Scroll Reveal (与首页一致)
- 交互反馈: Scale + Opacity 变化

---

## 6. 图片资源规划

### 6.1 设备类型图标
- 使用 Lucide Vue Next 图标库 (已有依赖)
- 设备类型映射:
  - 主控: `Cpu` 图标
  - 从机: `Smartphone` 图标
  - 灯光: `Lightbulb` 图标
  - 继电器: `Zap` 图标
  - 舵机: `RotateCcw` 图标

### 6.2 空状态插图
- 使用纯 CSS/SVG 绘制的简约插图
- 风格: Apple 风格线条插画
- 颜色: 使用 `--apple-text-tertiary` 保持低调

### 6.3 背景装饰
- 设备详情页头部可添加微妙的渐变光晕效果
- 使用 CSS `radial-gradient` 实现，不依赖外部图片

---

## 7. 组件清单

### 7.1 新增组件
| 组件名 | 路径 | 职责 |
|--------|------|------|
| DeviceDetailView | `views/DeviceDetailView.vue` | 设备详情页主视图 |
| DeviceHeroCard | `components/DeviceHeroCard.vue` | 设备信息头部卡片 |
| DeviceGpioGrid | `components/DeviceGpioGrid.vue` | GPIO状态总览网格 |
| DeviceControlPanel | `components/DeviceControlPanel.vue` | 控制面板容器 |
| DeviceLogPanel | `components/DeviceLogPanel.vue` | 设备专属日志 |
| EmptyState | `components/EmptyState.vue` | 空状态页面 |

### 7.2 复用组件
| 组件名 | 来源 | 用途 |
|--------|------|------|
| LightControl | `components/LightControl.vue` | 灯光开关控制 |
| RelayControl | `components/RelayControl.vue` | 继电器开关控制 |
| ServoControl | `components/ServoControl.vue` | 舵机角度控制 |
| StatusBadge | `components/StatusBadge.vue` | 在线状态指示 |

### 7.3 修改组件
| 组件名 | 修改内容 |
|--------|----------|
| App.vue | 添加 `<router-view>`，替换静态内容为路由视图 |
| DeviceCard.vue | 添加点击跳转入口，添加 hover 效果提示可点击 |
| main.ts | 注册 Vue Router |

---

## 8. 文件变更清单

### 8.1 新增文件
```
packages/frontend/src/router/index.ts          # 路由配置
packages/frontend/src/views/HomeView.vue        # 首页视图 (从 App.vue 提取)
packages/frontend/src/views/DeviceDetailView.vue # 设备详情视图
packages/frontend/src/components/DeviceHeroCard.vue
packages/frontend/src/components/DeviceGpioGrid.vue
packages/frontend/src/components/DeviceControlPanel.vue
packages/frontend/src/components/DeviceLogPanel.vue
packages/frontend/src/components/EmptyState.vue
```

### 8.2 修改文件
```
packages/frontend/src/App.vue                   # 添加路由视图 + 全局导航
packages/frontend/src/main.ts                   # 注册 Vue Router
packages/frontend/src/components/DeviceCard.vue # 添加点击跳转
```

---

## 9. 兼容性要求

- **Vue Router 4**: 与 Vue 3 组合式 API 兼容
- **Pinia**: 保持现有 store 结构不变
- **WebSocket**: 全局单例，详情页复用现有连接
- **响应式**: 支持移动端和桌面端自适应
- **PWA**: 路由切换不影响 Service Worker 缓存策略
