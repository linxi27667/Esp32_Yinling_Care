# IoT 控制面板前后端开发计划

## 项目概述

为"银龄智享"智慧养老系统构建一个基于 Web 的 IoT 控制面板，实现与 ESP32 硬件设备的实时双向通信。采用 Monorepo 结构，前端使用 Vue 3 + Vite + TypeScript + Material Design 3 风格，后端使用 Node.js + Express + WebSocket + MQTT。

## 技术栈

- **前端**: Vue 3 + Vite + TypeScript + Pinia + VueUse + TailwindCSS + Material Symbols
- **后端**: Node.js + Express + WebSocket (ws) + MQTT.js (async-mqtt)
- **通信**: WebSocket (前后端) + MQTT (后端与硬件)
- **构建**: Vite (前端) + tsx/ts-node (后端)

## 项目结构 (Monorepo)

```
xiaozhi-esp32-main/
├── web-panel/                          # 单体仓库根目录
│   ├── package.json                    # 根 package.json (workspaces)
│   ├── tsconfig.json                   # 共享 TS 配置
│   ├── README.md
│   │
│   ├── packages/
│   │   ├── shared/                     # 共享类型与常量
│   │   │   ├── src/
│   │   │   │   ├── types.ts            # 前后端共享的 TypeScript 类型
│   │   │   │   ├── constants.ts        # MQTT Topic、命令码、设备映射
│   │   │   │   └── index.ts
│   │   │   ├── package.json
│   │   │   └── tsconfig.json
│   │   │
│   │   ├── backend/                    # 后端服务
│   │   │   ├── src/
│   │   │   │   ├── index.ts            # 入口：启动 Express + WebSocket + MQTT
│   │   │   │   ├── server.ts           # Express 应用配置
│   │   │   │   ├── websocket.ts        # WebSocket 服务器：管理前端连接、广播状态
│   │   │   │   ├── mqtt-client.ts      # MQTT 客户端：连接 Broker、订阅 Topic、处理消息
│   │   │   │   ├── device-manager.ts   # 设备管理：维护设备列表、在线状态、GPIO状态
│   │   │   │   ├── command-router.ts   # 命令路由：将前端指令转为 MQTT 命令包
│   │   │   │   ├── api-routes.ts       # REST API 路由
│   │   │   │   └── utils.ts
│   │   │   ├── package.json
│   │   │   └── tsconfig.json
│   │   │
│   │   └── frontend/                   # 前端应用
│   │       ├── public/
│   │       ├── src/
│   │       │   ├── main.ts             # Vue 应用入口
│   │       │   ├── App.vue             # 根组件
│   │       │   ├── router.ts           # Vue Router
│   │       │   ├── stores/             # Pinia 状态管理
│   │       │   │   ├── deviceStore.ts  # 设备状态、在线状态、GPIO状态
│   │       │   │   ├── wsStore.ts      # WebSocket 连接管理
│   │       │   │   └── scenarioStore.ts# 场景状态
│   │       │   ├── components/         # 组件
│   │       │   │   ├── DeviceCard.vue      # 设备卡片（一楼/二楼/三楼）
│   │       │   │   ├── LightControl.vue    # 灯光控制（带 Ripple 动效）
│   │       │   │   ├── RelayControl.vue    # 继电器控制
│   │       │   │   ├── ServoControl.vue    # 舵机角度滑块
│   │       │   │   ├── ScenarioPanel.vue   # 场景按钮面板
│   │       │   │   ├── SensorPanel.vue     # 传感器数据显示
│   │       │   │   ├── LogPanel.vue        # 操作日志
│   │       │   │   ├── StatusBadge.vue     # 在线/离线状态徽章
│   │       │   │   └── RippleButton.vue    # 通用水波纹按钮
│   │       │   ├── composables/        # Vue 组合式函数
│   │       │   │   ├── useWebSocket.ts
│   │       │   │   ├── useRipple.ts    # 水波纹效果
│   │       │   │   └── useAnimations.ts# 过渡动画
│   │       │   ├── styles/
│   │       │   │   └── material-theme.css  # Material Design 3 色彩与变量
│   │       │   └── utils/
│   │       │       └── formatters.ts
│   │       ├── index.html
│   │       ├── vite.config.ts
│   │       ├── package.json
│   │       └── tsconfig.json
│   │
│   └── pnpm-workspace.yaml             # pnpm workspaces 配置
```

## 核心功能模块

### 1. 实时双向通信

**前端 <-> 后端 (WebSocket)**
- 前端连接后端 WebSocket，实时接收设备状态更新
- 前端下发控制指令，后端通过 WebSocket 接收并转发到 MQTT
- 控制指令下发后，前端显示 Loading 状态，收到响应后显示 Success/Error

**后端 <-> 硬件 (MQTT)**
- 后端作为 MQTT 客户端连接 `broker.emqx.io:1883`
- 订阅 Topic:
  - `xiaozhi/iot/resp/+` — 从机响应
  - `xiaozhi/iot/heartbeat/+` — 从机心跳
  - `xiaozhi/iot/announce/+` — 从机上线声明
- 发布 Topic:
  - `xiaozhi/iot/cmd/broadcast` — 广播命令
  - `xiaozhi/iot/cmd/{device_id}` — 单播命令

### 2. 设备状态实时显示

- **设备卡片**: 一楼 / 二楼 / 三楼，显示在线状态（绿色/灰色脉冲动画）
- **GPIO 状态**: 灯光 ON/OFF（带颜色脉动）、继电器状态、舵机角度
- **本地 GPIO**: 主机本地 GPIO 状态（led1, led2, relay1, relay2）
- **传感器数据**: 三楼烟雾、雨滴传感器（预留，从机本地处理暂不上报）

### 3. 设备控制

- **灯光控制**: 点击切换 ON/OFF，带 Ripple 水波纹反馈、颜色过渡动画
- **继电器控制**: 开关按钮，带状态反馈
- **舵机控制**: 滑块调节角度（0°~180°），实时下发，带 Loading/Success 状态
- **批量控制**: 全开、全关、全灯开、全灯关

### 4. 场景联动

- **回家场景**: 打开大门 + 所有灯光
- **离家场景**: 关闭大门
- **晚安场景**: 关闭所有灯光
- **紧急场景**: 触发紧急广播
- **雨天收衣**: 三楼晾衣架收起
- **通风场景**: 开窗 + 开风扇

### 5. 操作日志

- 实时显示所有控制操作、设备响应、心跳记录
- 支持按设备/操作类型筛选
- 时间戳格式化显示

## 数据流设计

```
┌──────────────┐      WebSocket       ┌──────────────┐      MQTT       ┌──────────────┐
│   前端页面    │ ◄──────────────────► │   后端服务    │ ◄────────────► │  MQTT Broker │
│  (Vue 3)     │   JSON 消息          │  (Node.js)   │   二进制包      │ (emqx.io)   │
└──────────────┘                      └──────────────┘                 └──────┬───────┘
      ▲                                                                        │
      │  1. 用户点击"打开一楼灯"                                                │
      │  2. 前端发送 WS: {type:'cmd', device:'1', gpio:0, value:1}              │
      │  3. 后端构造 MQTT 命令包 {cmd:0x01, device_id:1, gpio_index:0, value:1} │
      │  4. 后端 Publish → xiaozhi/iot/cmd/1                                    │
      │  5. 一楼从机收到 → 执行 → 发送响应                                       │
      │  6. 后端收到 resp → 解析 → WS 广播给所有前端                             │
      │  7. 前端更新状态 → 显示 Success 动画                                     │
      │                                                                        │
      │  心跳流: 从机每30s发送 heartbeat/{mac} → 后端更新在线状态 → WS 广播      │
```

## WebSocket 消息协议 (前后端)

```typescript
// 前端 -> 后端
interface WsCommand {
  type: 'command';
  payload: {
    deviceId: string;      // '1', '2', '3', 'broadcast'
    command: 'set_gpio' | 'set_light' | 'set_relay' | 'set_servo' | 'scenario';
    gpioIndex?: number;    // 0~15
    value?: number;        // 0/1 或角度 0~180
    scenario?: 'homecoming' | 'leavehome' | 'goodnight' | 'emergency' | 'rain' | 'ventilate';
  };
}

// 后端 -> 前端 (状态广播)
interface WsStateUpdate {
  type: 'state_update';
  payload: {
    devices: DeviceState[];
    localGpios: GpioState[];
    timestamp: number;
  };
}

// 后端 -> 前端 (设备响应)
interface WsDeviceResponse {
  type: 'device_response';
  payload: {
    deviceId: string;
    mac: string;
    command: number;
    gpioIndex: number;
    value: number;
    success: boolean;
  };
}

// 后端 -> 前端 (日志)
interface WsLog {
  type: 'log';
  payload: {
    level: 'info' | 'warn' | 'error';
    message: string;
    timestamp: number;
  };
}
```

## 关键实现步骤

### Phase 1: 基础架构搭建
1. 初始化 Monorepo 结构 (pnpm workspaces)
2. 配置 `packages/shared` 共享类型和常量
3. 搭建后端 Express + WebSocket 基础框架
4. 搭建前端 Vue 3 + Vite + TailwindCSS 基础框架
5. 配置开发脚本 (同时启动前后端)

### Phase 2: MQTT 通信层
1. 后端实现 MQTT 客户端连接 Broker
2. 订阅所有相关 Topic，解析二进制命令包
3. 实现设备管理器：维护设备列表、在线状态、GPIO 状态
4. 实现命令路由器：将前端指令转换为 MQTT 二进制包
5. 心跳超时检测机制

### Phase 3: WebSocket 实时通信
1. 后端 WebSocket 服务器管理前端连接
2. 实现消息广播：设备状态变化时推送给所有前端
3. 前端 WebSocket 连接管理 (自动重连、心跳)
4. Pinia Store 设计：设备状态、连接状态、日志

### Phase 4: 前端 UI 开发
1. Material Design 3 主题配置 (色彩、阴影、圆角)
2. 通用组件：RippleButton、StatusBadge、DeviceCard
3. 设备控制组件：LightControl、RelayControl、ServoControl
4. 场景面板：ScenarioPanel
5. 传感器面板：SensorPanel
6. 日志面板：LogPanel
7. 交互动效：水波纹、状态切换过渡、Loading/Success 反馈

### Phase 5: 集成测试
1. 前后端联调：确保指令下发和状态回传闭环
2. 与真实硬件联调：连接 MQTT Broker，测试实际控制
3. 性能优化：WebSocket 重连、MQTT 断线重连、UI 渲染优化

## Material Design 3 动效规范

- **Ripple 水波纹**: 点击时从触点扩散的圆形波纹，持续 300ms，ease-out
- **状态切换过渡**: 颜色变化使用 200ms cubic-bezier(0.4, 0, 0.2, 1)
- **卡片悬浮**: hover 时 elevation 提升，阴影扩散，200ms 过渡
- **Loading 状态**: 圆形进度指示器，Material 标准旋转动画
- **Success 反馈**: 绿色勾选图标 + 轻微缩放弹跳 (scale 0.8 -> 1.0, 300ms)
- **在线状态脉动**: 绿色圆点 + 扩散光环动画 (pulse, 2s infinite)

## 运行方式

```bash
# 根目录安装依赖
cd web-panel
pnpm install

# 开发模式（同时启动前后端）
pnpm dev

# 单独启动后端
cd packages/backend
pnpm dev

# 单独启动前端
cd packages/frontend
pnpm dev
```

## 与现有硬件的兼容性

- **MQTT Broker**: 默认使用 `broker.emqx.io:1883`（与硬件端一致）
- **Topic 格式**: 完全兼容现有 `xiaozhi/iot/*` Topic 体系
- **命令包格式**: 使用 `iot_command_packet_t` 二进制结构（8字节）
- **设备发现**: 支持从机 ANNOUNCE_V2 自动发现，无需预配 MAC
- **心跳机制**: 兼容现有 30s 心跳 + 60s 超时检测
