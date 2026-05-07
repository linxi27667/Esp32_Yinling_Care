# Apple 风格 UI 重构计划

## 一、项目现状分析

当前前端基于 **Material Design 3** 风格，存在以下视觉问题：
- 色彩过于饱和（紫色主色调 `#6750A4`）
- 阴影层级过重，卡片浮动感过强
- 圆角和间距缺乏统一节奏
- 动画为线性过渡，缺乏物理质感
- 字体使用 Roboto + Noto Sans SC，缺乏高级感

## 二、重构目标

在**完全保留现有业务逻辑**（WebSocket 通信、Pinia 状态管理、设备控制指令）的前提下，将 UI 升级为 **Apple 官网级别**的视觉体验：

- **冷峻克制**的色调（深空灰、银白、纯黑）
- **毛玻璃**（Glassmorphism）材质背景
- **Bento Box** 网格卡片布局
- **弹簧物理**（Spring Physics）动效
- **大面积留白**与精致排版

## 三、实施步骤

### Phase 1: 全局样式系统重构

#### Task 1.1: 替换 CSS 变量系统

**文件：** `src/styles/apple-theme.css`

- 移除 Material Design 色彩变量
- 引入 Apple 风格变量：
  - `--apple-bg-primary`: `#000000` (深色模式) / `#F5F5F7` (浅色模式)
  - `--apple-bg-secondary`: `rgba(28, 28, 30, 0.6)` 配合 `backdrop-filter: blur(20px)`
  - `--apple-text-primary`: `#FFFFFF` / `#1D1D1F`
  - `--apple-text-secondary`: `rgba(255, 255, 255, 0.6)` / `rgba(0, 0, 0, 0.5)`
  - `--apple-accent`: `#007AFF` (蓝色点缀)
  - `--apple-success`: `#34C759`
  - `--apple-warning`: `#FF9500`
  - `--apple-danger`: `#FF3B30`
- 字体栈改为：`-apple-system, BlinkMacSystemFont, "SF Pro Display", "SF Pro Text", "Helvetica Neue", sans-serif`

#### Task 1.2: 创建弹簧动效工具类

**文件：** `src/styles/apple-animations.css`

```css
/* 弹簧缓动函数 */
--apple-spring: cubic-bezier(0.34, 1.56, 0.64, 1);
--apple-spring-soft: cubic-bezier(0.25, 1, 0.5, 1);
--apple-ease-out: cubic-bezier(0.16, 1, 0.3, 1);

/* 卡片悬浮 */
.apple-card {
  transition: transform 0.4s var(--apple-spring-soft),
              box-shadow 0.4s var(--apple-ease-out);
}
.apple-card:hover {
  transform: scale(1.01);
  box-shadow: 0 8px 32px rgba(0, 0, 0, 0.12);
}

/* 毛玻璃效果 */
.apple-glass {
  background: rgba(255, 255, 255, 0.08);
  backdrop-filter: blur(20px) saturate(180%);
  -webkit-backdrop-filter: blur(20px) saturate(180%);
  border: 1px solid rgba(255, 255, 255, 0.1);
}
```

### Phase 2: 布局系统重构（Bento Box）

#### Task 2.1: 重写 App.vue 主布局

**布局策略：**
- 顶部导航栏改为**超薄毛玻璃**（高度 48px，`backdrop-filter: blur(20px)`）
- 统计概览改为**横向 Bento 条**：4 个等宽卡片，大圆角（24px），无边框，仅靠背景色区分
- 场景面板改为**2x3 Bento 网格**，每个场景是一个正方形卡片，hover 时轻微上浮
- 设备控制区改为**垂直 Bento 列表**，每个设备卡片占满宽度，内部采用网格排列 GPIO
- 传感器和日志并排在底部，各占 50%

**关键 CSS：**
```css
.bento-grid {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 16px;
}

.bento-card {
  border-radius: 24px;
  background: linear-gradient(135deg, rgba(255,255,255,0.1), rgba(255,255,255,0.05));
  padding: 24px;
  position: relative;
  overflow: hidden;
}
```

### Phase 3: 组件级视觉重构

#### Task 3.1: 重写 DeviceCard.vue

**改造点：**
- 卡片背景改为**深空灰渐变**（`linear-gradient(180deg, #1C1C1E, #000000)`）
- 设备 ID 徽章改为**纯白色圆形**，字体使用 SF Pro Display，字重 600
- 在线状态指示器改为**脉冲光晕**（`box-shadow` 动画，非绿色圆点）
- GPIO 控制区采用**网格布局**（3 列），每个控制项是一个小 Bento 单元
- 灯光开关改为**Apple 风格滑块**（圆角矩形轨道，白色圆形滑块）

#### Task 3.2: 重写 LightControl.vue

**改造点：**
- 移除 Material 的圆形图标背景
- 改为**纯文字 + 滑块**的极简布局
- 滑块轨道：`height: 32px`, `border-radius: 16px`, 背景色根据状态变化（关闭时 `#333`, 开启时 `#007AFF`）
- 滑块按钮：`32px` 圆形，白色，带阴影
- 切换动画使用弹簧物理：`transition: transform 0.5s cubic-bezier(0.34, 1.56, 0.64, 1)`

#### Task 3.3: 重写 ScenarioPanel.vue

**改造点：**
- 6 个场景卡片改为**正方形 Bento 卡片**
- 每个卡片背景使用**微妙的渐变**（如回家场景用暖色调渐变）
- 图标改为**线条风格**（SVG），非 Material Symbols
- Hover 效果：**缩放 1.02 + 背景亮度提升**，无阴影变化

#### Task 3.4: 重写 StatusBadge.vue

**改造点：**
- 移除背景色块和边框
- 改为**纯文字 + 脉冲圆点**
- 在线状态：白色圆点 + `box-shadow: 0 0 8px rgba(52, 199, 89, 0.6)` 脉冲动画
- 离线状态：灰色圆点，无动画

#### Task 3.5: 重写 LogPanel.vue

**改造点：**
- 日志项移除背景色块
- 改为**细线分隔**的列表
- 时间戳使用**等宽字体**（SF Mono）
- 错误日志左侧增加**红色竖线**指示器，非背景色

### Phase 4: 动效系统升级

#### Task 4.1: 创建 useSpringAnimation 组合式函数

**文件：** `src/composables/useSpringAnimation.ts`

```typescript
export function useSpringAnimation() {
  const springConfig = {
    stiffness: 100,
    damping: 15,
    mass: 1,
  };

  function springTransition(duration: number = 400) {
    return `all ${duration}ms cubic-bezier(0.34, 1.56, 0.64, 1)`;
  }

  return { springConfig, springTransition };
}
```

#### Task 4.2: 添加滚动触发动画

**文件：** `src/composables/useScrollReveal.ts`

使用 Intersection Observer 实现元素进入视口时的**淡入 + 上浮**动画：
```css
.scroll-reveal {
  opacity: 0;
  transform: translateY(30px);
  transition: opacity 0.8s var(--apple-ease-out),
              transform 0.8s var(--apple-ease-out);
}
.scroll-reveal.visible {
  opacity: 1;
  transform: translateY(0);
}
```

### Phase 5: 字体与图标替换

#### Task 5.1: 替换字体

**修改：** `index.html`
- 移除 Google Fonts（Roboto、Noto Sans SC）
- 使用系统字体栈：`-apple-system, BlinkMacSystemFont, "SF Pro Text", "Helvetica Neue", "PingFang SC", "Microsoft YaHei", sans-serif`

#### Task 5.2: 替换图标

**方案：** 使用 **Lucide Icons**（线条风格，更接近 Apple 设计）

```bash
npm install lucide-vue-next
```

替换所有 `material-symbols-outlined` 为对应的 Lucide 图标：
- `smart_home` → `Home`
- `lightbulb` → `Lightbulb`
- `power` → `Power`
- `sync_alt` → `RotateCcw`
- `auto_awesome` → `Sparkles`
- `receipt_long` → `ScrollText`
- `sensors` → `Activity`

### Phase 6: 深色/浅色模式支持

#### Task 6.1: 添加 CSS 变量媒体查询

```css
@media (prefers-color-scheme: dark) {
  :root {
    --apple-bg-primary: #000000;
    --apple-text-primary: #FFFFFF;
    /* ... */
  }
}

@media (prefers-color-scheme: light) {
  :root {
    --apple-bg-primary: #F5F5F7;
    --apple-text-primary: #1D1D1F;
    /* ... */
  }
}
```

## 四、文件变更清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `src/styles/material-theme.css` | 重命名/替换 | 改为 `apple-theme.css` |
| `src/styles/apple-animations.css` | 新增 | 弹簧动效工具类 |
| `src/composables/useSpringAnimation.ts` | 新增 | 弹簧动画组合式函数 |
| `src/composables/useScrollReveal.ts` | 新增 | 滚动触发动画 |
| `src/App.vue` | 修改 | Bento 布局重构 |
| `src/components/DeviceCard.vue` | 修改 | 毛玻璃卡片 + 网格 GPIO |
| `src/components/LightControl.vue` | 修改 | Apple 风格滑块 |
| `src/components/RelayControl.vue` | 修改 | 极简按钮 |
| `src/components/ServoControl.vue` | 修改 | 精致滑块 |
| `src/components/ScenarioPanel.vue` | 修改 | Bento 网格卡片 |
| `src/components/LogPanel.vue` | 修改 | 细线列表 |
| `src/components/SensorPanel.vue` | 修改 | 极简传感器卡片 |
| `src/components/StatusBadge.vue` | 修改 | 脉冲光晕指示器 |
| `src/components/RippleButton.vue` | 删除 | 替换为 Apple 风格按钮 |
| `src/main.ts` | 修改 | 引入新样式 |
| `index.html` | 修改 | 替换字体引用 |
| `package.json` | 修改 | 添加 lucide-vue-next 依赖 |

## 五、业务逻辑保护清单

以下逻辑**严禁修改**，仅做 UI 绑定：

- [ ] `useWebSocket.ts` — WebSocket 连接、消息处理、自动重连
- [ ] `deviceStore.ts` — Pinia 状态、设备数据、日志管理
- [ ] `App.vue` — `handleToggle`、`handleServoChange`、`handleScenario` 函数
- [ ] `DeviceCard.vue` — `toggle`、`servoChange` emit 事件
- [ ] `LightControl.vue` — `toggle` emit 事件和 pending 状态逻辑
- [ ] `ServoControl.vue` — `change` emit 事件和滑块值逻辑

## 六、成功标准

| 指标 | 目标 |
|------|------|
| 视觉风格 | 与 Apple 官网/系统设置界面一致 |
| 动画质感 | 所有过渡使用弹簧物理，无线性动画 |
| 布局 | 采用 Bento Box 网格，留白充足 |
| 功能完整性 | 所有原有控制功能正常工作 |
| 性能 | Lighthouse 性能分数 > 90 |
| 兼容性 | 保留 WebSocket 实时通信能力 |
