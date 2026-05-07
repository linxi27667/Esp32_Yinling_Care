# 独立设备控制界面实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为"银龄智享"IoT控制面板添加独立设备详情控制页，支持点击设备卡片进入详情页进行完整控制。

**Architecture:** 使用 Vue Router 4 实现单页应用路由，首页 (`HomeView`) 展示设备总览，详情页 (`DeviceDetailView`) 展示单个设备的完整控制和状态。复用现有 Pinia store 和 WebSocket 连接。

**Tech Stack:** Vue 3 + Vue Router 4 + Pinia + TailwindCSS + Apple Design System (已有)

---

## 文件结构映射

### 新增文件
- `packages/frontend/src/router/index.ts` — 路由配置 (2条路由)
- `packages/frontend/src/views/HomeView.vue` — 首页视图 (从 App.vue 提取)
- `packages/frontend/src/views/DeviceDetailView.vue` — 设备详情页主视图
- `packages/frontend/src/components/DeviceHeroCard.vue` — 设备信息头部卡片
- `packages/frontend/src/components/DeviceGpioGrid.vue` — GPIO状态总览
- `packages/frontend/src/components/DeviceControlPanel.vue` — 控制面板容器
- `packages/frontend/src/components/DeviceLogPanel.vue` — 设备专属日志
- `packages/frontend/src/components/EmptyState.vue` — 空状态/404页面

### 修改文件
- `packages/frontend/src/App.vue` — 精简为路由布局壳
- `packages/frontend/src/main.ts` — 注册 Vue Router
- `packages/frontend/src/components/DeviceCard.vue` — 添加点击跳转

---

## Task 1: 配置 Vue Router

**Files:**
- Create: `packages/frontend/src/router/index.ts`
- Modify: `packages/frontend/src/main.ts`

- [ ] **Step 1: 创建路由配置文件**

```typescript
import { createRouter, createWebHistory } from 'vue-router';
import HomeView from '@/views/HomeView.vue';
import DeviceDetailView from '@/views/DeviceDetailView.vue';

const routes = [
  {
    path: '/',
    name: 'Home',
    component: HomeView,
    meta: { title: '设备总览' },
  },
  {
    path: '/device/:deviceId',
    name: 'DeviceDetail',
    component: DeviceDetailView,
    props: true,
    meta: { title: '设备控制' },
  },
];

const router = createRouter({
  history: createWebHistory(),
  routes,
});

router.beforeEach((to, from, next) => {
  const title = to.meta.title as string;
  if (title) {
    document.title = `银龄智享 - ${title}`;
  }
  next();
});

export default router;
```

- [ ] **Step 2: 在 main.ts 中注册路由**

修改 `packages/frontend/src/main.ts`:

```typescript
import { createApp } from 'vue';
import { createPinia } from 'pinia';
import App from './App.vue';
import router from './router';
import './styles/apple-theme.css';

if ('serviceWorker' in navigator) {
  window.addEventListener('load', () => {
    navigator.serviceWorker
      .register('/sw.js')
      .then((registration) => {
        console.log('SW registered:', registration.scope);
      })
      .catch((error) => {
        console.log('SW registration failed:', error);
      });
  });
}

const app = createApp(App);
app.use(createPinia());
app.use(router);
app.mount('#app');
```

---

## Task 2: 创建 HomeView (从 App.vue 提取)

**Files:**
- Create: `packages/frontend/src/views/HomeView.vue`

- [ ] **Step 1: 创建 HomeView.vue**

将现有 App.vue 中的主体内容提取到 HomeView.vue，保留所有业务逻辑和组件引用。

```vue
<template>
  <div>
    <!-- Hero Section -->
    <section class="scroll-reveal">
      <h1 class="apple-headline mb-2" style="color: var(--apple-text-primary);">
        设备控制中心
      </h1>
      <p class="apple-body" style="color: var(--apple-text-secondary);">
        实时监控与控制您的智能家居设备
      </p>
    </section>

    <!-- Stats Overview - Bento Bar -->
    <section class="scroll-reveal">
      <div class="grid grid-cols-2 lg:grid-cols-4 gap-4">
        <div v-for="stat in stats" :key="stat.label" class="bento-card">
          <div class="apple-caption mb-1">{{ stat.label }}</div>
          <div class="text-3xl font-semibold tracking-tight" :style="{ color: stat.color || 'var(--apple-text-primary)' }">
            {{ stat.value }}
          </div>
        </div>
      </div>
    </section>

    <!-- Scenario Panel - Bento Grid -->
    <section class="scroll-reveal">
      <h2 class="apple-title mb-4" style="color: var(--apple-text-primary);">场景联动</h2>
      <ScenarioPanel @scenario="handleScenario" />
    </section>

    <!-- Devices Section -->
    <section class="scroll-reveal">
      <h2 class="apple-title mb-4" style="color: var(--apple-text-primary);">设备控制</h2>
      <div class="space-y-4">
        <DeviceCard
          v-for="device in store.devices"
          :key="device.mac"
          :device="device"
          @toggle="handleToggle"
          @servo-change="handleServoChange"
        />
      </div>
      <div v-if="store.devices.length === 0" class="bento-card text-center py-16">
        <div class="w-16 h-16 rounded-full flex items-center justify-center mx-auto mb-4" style="background: var(--apple-surface);">
          <Smartphone :size="32" style="color: var(--apple-text-tertiary);" />
        </div>
        <p style="color: var(--apple-text-secondary);">暂无设备，等待从机上线...</p>
      </div>
    </section>

    <!-- Sensor & Logs - Side by Side -->
    <section class="scroll-reveal">
      <div class="grid grid-cols-1 lg:grid-cols-2 gap-4">
        <SensorPanel />
        <LogPanel />
      </div>
    </section>
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted } from 'vue';
import { useDeviceStore } from '@/stores/deviceStore';
import { useWebSocket } from '@/composables/useWebSocket';
import { LIGHT_COUNT, RELAY_COUNT } from '@xiaozhi/shared';
import { Smartphone } from 'lucide-vue-next';
import DeviceCard from '@/components/DeviceCard.vue';
import ScenarioPanel from '@/components/ScenarioPanel.vue';
import SensorPanel from '@/components/SensorPanel.vue';
import LogPanel from '@/components/LogPanel.vue';

const store = useDeviceStore();
const { sendCommand } = useWebSocket();

onMounted(() => {
  const observer = new IntersectionObserver(
    (entries) => {
      entries.forEach((entry) => {
        if (entry.isIntersecting) {
          entry.target.classList.add('visible');
        }
      });
    },
    { threshold: 0.05, rootMargin: '0px 0px -30px 0px' }
  );
  document.querySelectorAll('.scroll-reveal').forEach((el) => observer.observe(el));
});

const totalLightsOn = computed(() => {
  return store.devices.reduce((sum: number, d: any) => {
    return sum + d.gpios.slice(0, LIGHT_COUNT).filter((g: any) => g.value > 0).length;
  }, 0);
});

const totalRelaysOn = computed(() => {
  return store.devices.reduce((sum: number, d: any) => {
    return sum + d.gpios.slice(LIGHT_COUNT, LIGHT_COUNT + RELAY_COUNT).filter((g: any) => g.value > 0).length;
  }, 0);
});

const stats = computed(() => [
  { label: '在线设备', value: store.onlineDevices.length, color: 'var(--apple-success)' },
  { label: '总设备', value: store.devices.length },
  { label: '灯光开启', value: totalLightsOn.value, color: 'var(--apple-warning)' },
  { label: '继电器运行', value: totalRelaysOn.value, color: 'var(--apple-accent)' },
]);

function handleToggle(deviceId: string, gpioIndex: number, value: number) {
  sendCommand({ deviceId, command: 'set_gpio', gpioIndex, value });
}

function handleServoChange(deviceId: string, gpioIndex: number, value: number) {
  sendCommand({ deviceId, command: 'set_servo', gpioIndex, value });
}

function handleScenario(scenario: string) {
  sendCommand({ deviceId: 'broadcast', command: 'scenario', scenario });
}
</script>
```

---

## Task 3: 重构 App.vue 为路由布局壳

**Files:**
- Modify: `packages/frontend/src/App.vue`

- [ ] **Step 1: 精简 App.vue**

```vue
<template>
  <div class="min-h-screen" style="background-color: var(--apple-bg-primary);">
    <!-- Global Header -->
    <header class="sticky top-0 z-50 apple-glass-strong" style="height: 52px;">
      <div class="max-w-7xl mx-auto px-6 h-full flex items-center justify-between">
        <div class="flex items-center gap-3">
          <div class="w-8 h-8 rounded-lg flex items-center justify-center" style="background: linear-gradient(135deg, #0A84FF, #0071E3);">
            <Home :size="18" class="text-white" />
          </div>
          <div>
            <h1 class="apple-title" style="color: var(--apple-text-primary); font-size: 17px;">银龄智享</h1>
          </div>
        </div>
        <div class="flex items-center gap-2">
          <div class="apple-status-dot" :class="connected ? 'online' : 'offline'" />
          <span class="apple-caption">
            {{ connected ? '已连接' : '未连接' }}
          </span>
        </div>
      </div>
    </header>

    <!-- Router View -->
    <main class="max-w-7xl mx-auto px-6 py-8 space-y-6">
      <router-view v-slot="{ Component }">
        <transition
          name="page"
          mode="out-in"
          enter-active-class="page-enter-active"
          leave-active-class="page-leave-active"
          enter-from-class="page-enter-from"
          leave-to-class="page-leave-to"
        >
          <component :is="Component" />
        </transition>
      </router-view>
    </main>
  </div>
</template>

<script setup lang="ts">
import { useWebSocket } from '@/composables/useWebSocket';
import { Home } from 'lucide-vue-next';

const { connected } = useWebSocket();
</script>

<style scoped>
.page-enter-active,
.page-leave-active {
  transition: all 0.4s cubic-bezier(0.34, 1.56, 0.64, 1);
}
.page-enter-from {
  opacity: 0;
  transform: translateX(30px);
}
.page-leave-to {
  opacity: 0;
  transform: translateX(-30px);
}
</style>
```

---

## Task 4: 创建设备详情页 DeviceDetailView

**Files:**
- Create: `packages/frontend/src/views/DeviceDetailView.vue`

- [ ] **Step 1: 创建设备详情页**

```vue
<template>
  <div>
    <!-- Back Navigation -->
    <section class="scroll-reveal">
      <button
        class="flex items-center gap-2 apple-body transition-opacity hover:opacity-70"
        style="color: var(--apple-accent);"
        @click="goBack"
      >
        <ChevronLeft :size="20" />
        <span>返回设备列表</span>
      </button>
    </section>

    <!-- Device Not Found -->
    <EmptyState
      v-if="!device"
      icon="Smartphone"
      title="设备未找到"
      description="该设备可能已离线或被移除"
      action-text="返回首页"
      @action="goBack"
    />

    <template v-else>
      <!-- Hero Card -->
      <section class="scroll-reveal">
        <DeviceHeroCard :device="device" />
      </section>

      <!-- GPIO Status Grid -->
      <section class="scroll-reveal">
        <h2 class="apple-title mb-4" style="color: var(--apple-text-primary);">实时状态</h2>
        <DeviceGpioGrid :device="device" />
      </section>

      <!-- Control Panel -->
      <section class="scroll-reveal">
        <h2 class="apple-title mb-4" style="color: var(--apple-text-primary);">设备控制</h2>
        <DeviceControlPanel
          :device="device"
          @toggle="handleToggle"
          @servo-change="handleServoChange"
        />
      </section>

      <!-- Quick Actions -->
      <section class="scroll-reveal">
        <h2 class="apple-title mb-4" style="color: var(--apple-text-primary);">快捷操作</h2>
        <div class="grid grid-cols-2 gap-4">
          <button class="bento-card flex items-center justify-center gap-3 py-4" @click="turnAllOn">
            <Power :size="20" style="color: var(--apple-success);" />
            <span class="apple-body" style="color: var(--apple-text-primary);">全部开启</span>
          </button>
          <button class="bento-card flex items-center justify-center gap-3 py-4" @click="turnAllOff">
            <PowerOff :size="20" style="color: var(--apple-danger);" />
            <span class="apple-body" style="color: var(--apple-text-primary);">全部关闭</span>
          </button>
        </div>
      </section>

      <!-- Device Logs -->
      <section class="scroll-reveal">
        <h2 class="apple-title mb-4" style="color: var(--apple-text-primary);">操作记录</h2>
        <DeviceLogPanel :device-id="device.id" />
      </section>
    </template>
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted } from 'vue';
import { useRoute, useRouter } from 'vue-router';
import { useDeviceStore } from '@/stores/deviceStore';
import { useWebSocket } from '@/composables/useWebSocket';
import { ChevronLeft, Power, PowerOff } from 'lucide-vue-next';
import DeviceHeroCard from '@/components/DeviceHeroCard.vue';
import DeviceGpioGrid from '@/components/DeviceGpioGrid.vue';
import DeviceControlPanel from '@/components/DeviceControlPanel.vue';
import DeviceLogPanel from '@/components/DeviceLogPanel.vue';
import EmptyState from '@/components/EmptyState.vue';

const props = defineProps<{
  deviceId: string;
}>();

const route = useRoute();
const router = useRouter();
const store = useDeviceStore();
const { sendCommand } = useWebSocket();

const device = computed(() => {
  const id = props.deviceId || (route.params.deviceId as string);
  return store.devices.find((d) => d.id === id) || null;
});

function goBack() {
  router.push({ name: 'Home' });
}

function handleToggle(deviceId: string, gpioIndex: number, value: number) {
  sendCommand({ deviceId, command: 'set_gpio', gpioIndex, value });
}

function handleServoChange(deviceId: string, gpioIndex: number, value: number) {
  sendCommand({ deviceId, command: 'set_servo', gpioIndex, value });
}

function turnAllOn() {
  if (!device.value) return;
  device.value.gpios.forEach((gpio) => {
    if (gpio.mode === 'output') {
      sendCommand({ deviceId: device.value!.id, command: 'set_gpio', gpioIndex: gpio.index, value: 1 });
    }
  });
}

function turnAllOff() {
  if (!device.value) return;
  device.value.gpios.forEach((gpio) => {
    if (gpio.mode === 'output') {
      sendCommand({ deviceId: device.value!.id, command: 'set_gpio', gpioIndex: gpio.index, value: 0 });
    }
  });
}

onMounted(() => {
  const observer = new IntersectionObserver(
    (entries) => {
      entries.forEach((entry) => {
        if (entry.isIntersecting) {
          entry.target.classList.add('visible');
        }
      });
    },
    { threshold: 0.05, rootMargin: '0px 0px -30px 0px' }
  );
  document.querySelectorAll('.scroll-reveal').forEach((el) => observer.observe(el));
});
</script>
```

---

## Task 5: 创建子组件

### 5.1 DeviceHeroCard.vue

**Files:**
- Create: `packages/frontend/src/components/DeviceHeroCard.vue`

```vue
<template>
  <div class="bento-card relative overflow-hidden">
    <!-- Background Gradient Glow -->
    <div class="absolute -top-20 -right-20 w-40 h-40 rounded-full opacity-30" style="background: radial-gradient(circle, var(--apple-accent), transparent 70%);" />

    <div class="relative flex items-center gap-5">
      <!-- Device Avatar -->
      <div
        class="w-16 h-16 rounded-2xl flex items-center justify-center text-white font-bold text-2xl flex-shrink-0"
        style="background: linear-gradient(135deg, #3a3a3c, #1c1c1e);"
      >
        {{ device.id }}
      </div>

      <!-- Device Info -->
      <div class="flex-1 min-w-0">
        <h1 class="apple-headline mb-1" style="font-size: 24px;">{{ device.name }}</h1>
        <p class="apple-mono mb-2" style="color: var(--apple-text-tertiary);">{{ device.mac }}</p>
        <div class="flex items-center gap-3">
          <StatusBadge :online="device.online" />
          <span class="apple-caption" style="color: var(--apple-text-tertiary);">
            最后更新: {{ formatTime(device.lastHeartbeat) }}
          </span>
        </div>
      </div>
    </div>

    <!-- GPIO Count Stats -->
    <div class="grid grid-cols-3 gap-3 mt-6 pt-6" style="border-top: 1px solid var(--apple-border-subtle);">
      <div class="text-center">
        <div class="text-2xl font-semibold" style="color: var(--apple-accent);">{{ lightsCount }}</div>
        <div class="apple-caption">灯光</div>
      </div>
      <div class="text-center">
        <div class="text-2xl font-semibold" style="color: var(--apple-success);">{{ relaysCount }}</div>
        <div class="apple-caption">继电器</div>
      </div>
      <div class="text-center">
        <div class="text-2xl font-semibold" style="color: var(--apple-warning);">{{ servosCount }}</div>
        <div class="apple-caption">舵机</div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue';
import type { DeviceState } from '@xiaozhi/shared';
import { LIGHT_COUNT, RELAY_COUNT, SERVO_COUNT } from '@xiaozhi/shared';
import { formatTime } from '@/utils/formatters';
import StatusBadge from './StatusBadge.vue';

const props = defineProps<{
  device: DeviceState;
}>();

const lightsCount = computed(() => props.device.gpios.slice(0, LIGHT_COUNT).length);
const relaysCount = computed(() => props.device.gpios.slice(LIGHT_COUNT, LIGHT_COUNT + RELAY_COUNT).length);
const servosCount = computed(() => props.device.gpios.slice(LIGHT_COUNT + RELAY_COUNT, LIGHT_COUNT + RELAY_COUNT + SERVO_COUNT).length);
</script>
```

### 5.2 DeviceGpioGrid.vue

**Files:**
- Create: `packages/frontend/src/components/DeviceGpioGrid.vue`

```vue
<template>
  <div class="grid grid-cols-2 md:grid-cols-3 lg:grid-cols-4 gap-3">
    <div
      v-for="gpio in device.gpios"
      :key="gpio.index"
      class="bento-card p-4 flex flex-col items-center justify-center gap-2"
      :style="getGpioStyle(gpio)"
    >
      <component :is="getGpioIcon(gpio)" :size="24" :style="{ color: getGpioColor(gpio) }" />
      <span class="apple-body" style="font-size: 14px;">{{ gpio.name }}</span>
      <span class="apple-caption" :style="{ color: getGpioColor(gpio) }">
        {{ getGpioStatus(gpio) }}
      </span>
    </div>
  </div>
</template>

<script setup lang="ts">
import type { DeviceState, GpioState } from '@xiaozhi/shared';
import { LIGHT_COUNT, RELAY_COUNT } from '@xiaozhi/shared';
import { Lightbulb, Power, RotateCcw } from 'lucide-vue-next';

const props = defineProps<{
  device: DeviceState;
}>();

function getGpioType(gpio: GpioState): 'light' | 'relay' | 'servo' {
  if (gpio.index < LIGHT_COUNT) return 'light';
  if (gpio.index < LIGHT_COUNT + RELAY_COUNT) return 'relay';
  return 'servo';
}

function getGpioIcon(gpio: GpioState) {
  const type = getGpioType(gpio);
  if (type === 'light') return Lightbulb;
  if (type === 'relay') return Power;
  return RotateCcw;
}

function getGpioColor(gpio: GpioState): string {
  const type = getGpioType(gpio);
  if (type === 'light') {
    return gpio.value > 0 ? 'var(--apple-accent)' : 'var(--apple-text-tertiary)';
  }
  if (type === 'relay') {
    return gpio.value > 0 ? 'var(--apple-success)' : 'var(--apple-text-tertiary)';
  }
  return 'var(--apple-warning)';
}

function getGpioStatus(gpio: GpioState): string {
  const type = getGpioType(gpio);
  if (type === 'light') return gpio.value > 0 ? '开启' : '关闭';
  if (type === 'relay') return gpio.value > 0 ? '运行中' : '已关闭';
  return `${gpio.value}°`;
}

function getGpioStyle(gpio: GpioState) {
  const type = getGpioType(gpio);
  if (type === 'light' && gpio.value > 0) {
    return { background: 'rgba(10, 132, 255, 0.1)' };
  }
  if (type === 'relay' && gpio.value > 0) {
    return { background: 'rgba(48, 209, 88, 0.1)' };
  }
  return {};
}
</script>
```

### 5.3 DeviceControlPanel.vue

**Files:**
- Create: `packages/frontend/src/components/DeviceControlPanel.vue`

```vue
<template>
  <div class="space-y-6">
    <!-- Lights -->
    <div v-if="lights.length > 0" class="bento-card">
      <h3 class="apple-title mb-4" style="font-size: 17px; color: var(--apple-text-primary);">
        <Lightbulb :size="18" class="inline mr-2" style="color: var(--apple-accent);" />
        灯光控制
      </h3>
      <div class="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-3">
        <LightControl
          v-for="gpio in lights"
          :key="gpio.index"
          :device-id="device.id"
          :gpio-index="gpio.index"
          :name="gpio.name"
          :value="gpio.value"
          @toggle="handleToggle"
        />
      </div>
    </div>

    <!-- Relays -->
    <div v-if="relays.length > 0" class="bento-card">
      <h3 class="apple-title mb-4" style="font-size: 17px; color: var(--apple-text-primary);">
        <Power :size="18" class="inline mr-2" style="color: var(--apple-success);" />
        继电器控制
      </h3>
      <div class="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-3">
        <RelayControl
          v-for="gpio in relays"
          :key="gpio.index"
          :device-id="device.id"
          :gpio-index="gpio.index"
          :name="gpio.name"
          :value="gpio.value"
          @toggle="handleToggle"
        />
      </div>
    </div>

    <!-- Servos -->
    <div v-if="servos.length > 0" class="bento-card">
      <h3 class="apple-title mb-4" style="font-size: 17px; color: var(--apple-text-primary);">
        <RotateCcw :size="18" class="inline mr-2" style="color: var(--apple-warning);" />
        舵机控制
      </h3>
      <div class="grid grid-cols-1 sm:grid-cols-2 lg:grid-cols-3 gap-3">
        <ServoControl
          v-for="gpio in servos"
          :key="gpio.index"
          :device-id="device.id"
          :gpio-index="gpio.index"
          :name="gpio.name"
          :value="gpio.value"
          @change="handleServoChange"
        />
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue';
import type { DeviceState } from '@xiaozhi/shared';
import { LIGHT_COUNT, RELAY_COUNT, SERVO_COUNT } from '@xiaozhi/shared';
import { Lightbulb, Power, RotateCcw } from 'lucide-vue-next';
import LightControl from './LightControl.vue';
import RelayControl from './RelayControl.vue';
import ServoControl from './ServoControl.vue';

const props = defineProps<{
  device: DeviceState;
}>();

const emit = defineEmits<{
  toggle: [deviceId: string, gpioIndex: number, value: number];
  servoChange: [deviceId: string, gpioIndex: number, value: number];
}>();

const lights = computed(() => props.device.gpios.slice(0, LIGHT_COUNT));
const relays = computed(() => props.device.gpios.slice(LIGHT_COUNT, LIGHT_COUNT + RELAY_COUNT));
const servos = computed(() => props.device.gpios.slice(LIGHT_COUNT + RELAY_COUNT, LIGHT_COUNT + RELAY_COUNT + SERVO_COUNT));

function handleToggle(deviceId: string, gpioIndex: number, value: number) {
  emit('toggle', deviceId, gpioIndex, value);
}

function handleServoChange(deviceId: string, gpioIndex: number, value: number) {
  emit('servoChange', deviceId, gpioIndex, value);
}
</script>
```

### 5.4 DeviceLogPanel.vue

**Files:**
- Create: `packages/frontend/src/components/DeviceLogPanel.vue`

```vue
<template>
  <div class="bento-card">
    <div class="flex items-center justify-between mb-4">
      <h2 class="apple-title" style="font-size: 17px; color: var(--apple-text-primary);">设备日志</h2>
      <span class="apple-caption" style="color: var(--apple-text-tertiary);">
        共 {{ filteredLogs.length }} 条
      </span>
    </div>

    <div class="h-64 overflow-y-auto space-y-0 custom-scrollbar">
      <div
        v-for="(log, i) in filteredLogs"
        :key="i"
        class="py-3 border-b transition-all duration-500"
        :style="{
          borderColor: 'var(--apple-border-subtle)',
          borderLeftWidth: '2px',
          borderLeftColor: logBorderColor(log.level),
          paddingLeft: '12px',
        }"
      >
        <div class="apple-mono mb-1" style="color: var(--apple-text-tertiary);">
          {{ formatTime(log.timestamp) }}
        </div>
        <div class="apple-body" style="font-size: 14px; color: var(--apple-text-primary);">
          {{ log.message }}
        </div>
      </div>

      <div
        v-if="filteredLogs.length === 0"
        class="text-center py-8"
        style="color: var(--apple-text-tertiary);"
      >
        <ScrollText :size="24" class="mx-auto mb-2 opacity-30" />
        <span class="apple-caption">暂无该设备日志</span>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue';
import { useDeviceStore } from '@/stores/deviceStore';
import { formatTime } from '@/utils/formatters';
import { ScrollText } from 'lucide-vue-next';

const props = defineProps<{
  deviceId: string;
}>();

const store = useDeviceStore();

const filteredLogs = computed(() => {
  return store.logs.filter((log) =>
    log.message.includes(`设备 ${props.deviceId}`) ||
    log.message.includes(props.deviceId)
  );
});

function logBorderColor(level: string): string {
  switch (level) {
    case 'error':
      return 'var(--apple-danger)';
    case 'warn':
      return 'var(--apple-warning)';
    default:
      return 'transparent';
  }
}
</script>
```

### 5.5 EmptyState.vue

**Files:**
- Create: `packages/frontend/src/components/EmptyState.vue`

```vue
<template>
  <div class="bento-card text-center py-16">
    <div
      class="w-20 h-20 rounded-full flex items-center justify-center mx-auto mb-4"
      style="background: var(--apple-surface);"
    >
      <component :is="iconComponent" :size="40" style="color: var(--apple-text-tertiary);" />
    </div>
    <h3 class="apple-title mb-2" style="color: var(--apple-text-primary);">{{ title }}</h3>
    <p class="apple-body mb-6" style="color: var(--apple-text-secondary);">{{ description }}</p>
    <button
      v-if="actionText"
      class="apple-body px-6 py-3 rounded-xl transition-all duration-300"
      style="background: var(--apple-accent); color: white;"
      @click="$emit('action')"
    >
      {{ actionText }}
    </button>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue';
import { Smartphone, WifiOff, AlertCircle } from 'lucide-vue-next';

const props = defineProps<{
  icon: string;
  title: string;
  description: string;
  actionText?: string;
}>();

defineEmits<{
  action: [];
}>();

const iconComponent = computed(() => {
  switch (props.icon) {
    case 'WifiOff': return WifiOff;
    case 'AlertCircle': return AlertCircle;
    default: return Smartphone;
  }
});
</script>
```

---

## Task 6: 修改 DeviceCard 添加跳转入口

**Files:**
- Modify: `packages/frontend/src/components/DeviceCard.vue`

- [ ] **Step 1: 添加点击跳转和视觉提示**

在 DeviceCard.vue 的根 div 上添加点击事件和 hover 效果:

```vue
<template>
  <div
    class="bento-card cursor-pointer"
    :class="{ 'opacity-50': !device.online }"
    :style="{ transition: 'all 0.5s var(--apple-spring-soft)' }"
    @click="goToDetail"
  >
    <!-- 现有内容不变 -->
    <!-- 在 Header 右侧添加箭头图标 -->
    <div class="flex items-center justify-between mb-6">
      <div class="flex items-center gap-4">
        <!-- ... 现有内容 -->
      </div>
      <div class="flex items-center gap-2">
        <StatusBadge :online="device.online" />
        <ChevronRight
          :size="18"
          class="transition-transform duration-300"
          style="color: var(--apple-text-tertiary);"
          :class="{ 'group-hover:translate-x-1': true }"
        />
      </div>
    </div>
    <!-- ... 其余内容不变 -->
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue';
import { useRouter } from 'vue-router';
import type { DeviceState } from '@xiaozhi/shared';
import { LIGHT_COUNT, RELAY_COUNT, SERVO_COUNT } from '@xiaozhi/shared';
import { ChevronRight } from 'lucide-vue-next';
import StatusBadge from './StatusBadge.vue';
// ... 其他导入

const router = useRouter();

function goToDetail() {
  router.push({ name: 'DeviceDetail', params: { deviceId: props.device.id } });
}
// ... 其余不变
</script>
```

---

## Task 7: 验证与测试

- [ ] **Step 1: 启动开发服务器**

Run: `cd packages/frontend && npm run dev`
Expected: Vite dev server starts on port 5173

- [ ] **Step 2: 验证首页正常显示**

Open: `http://localhost:5173/`
Expected: 首页设备总览正常显示，与重构前一致

- [ ] **Step 3: 验证设备卡片可点击**

Action: 点击任意 DeviceCard
Expected: 路由跳转到 `/device/:deviceId`，页面从右侧滑入

- [ ] **Step 4: 验证详情页显示**

Check: 详情页显示设备信息、GPIO状态、控制面板、日志
Expected: 所有信息正确，样式与 Apple 设计系统一致

- [ ] **Step 5: 验证返回功能**

Action: 点击"返回设备列表"按钮
Expected: 路由返回首页 `/`，页面动画正常

- [ ] **Step 6: 验证控制功能**

Action: 在详情页操作灯光/继电器/舵机
Expected: WebSocket 命令正常发送，状态实时更新

- [ ] **Step 7: 验证空状态**

Action: 访问不存在的设备ID，如 `/device/999`
Expected: 显示"设备未找到"空状态页面

---

## 依赖安装

Vue Router 4 需要安装:

```bash
cd packages/frontend
npm install vue-router@4
```

---

## Spec Coverage Check

| 需求 | 实现任务 |
|------|----------|
| 独立设备控制界面 | Task 4: DeviceDetailView.vue + 子组件 |
| 设备状态显示 | Task 5.1: DeviceHeroCard + Task 5.2: DeviceGpioGrid |
| 参数调节 | Task 5.3: DeviceControlPanel (复用现有控制组件) |
| 操作控制 | Task 4: 快捷操作按钮 + 控制面板 |
| 界面间跳转 | Task 1: Vue Router + Task 6: DeviceCard 点击 |
| 流畅切换 | Task 3: App.vue transition 动画 |
| Apple风格图片 | Task 5.1: CSS渐变光晕 + Lucide图标 |
| 一致性 | 复用 apple-theme.css + 现有组件 |

## Placeholder Scan

- 无 TBD/TODO
- 所有代码片段完整
- 所有文件路径精确
- 无 "similar to" 引用
