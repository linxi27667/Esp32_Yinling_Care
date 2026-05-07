<template>
  <div class="home-container">
    <!-- Hero Stats -->
    <section class="hero-section">
      <div class="hero-header">
        <h1 class="hero-title">设备总览</h1>
        <p class="hero-subtitle">实时监控与智能控制</p>
      </div>
      <div class="stats-grid">
        <div class="stat-card" v-for="stat in stats" :key="stat.label">
          <div class="stat-icon" :style="{ background: stat.bg }">
            <component :is="stat.icon" :size="18" />
          </div>
          <div class="stat-info">
            <span class="stat-value" :style="{ color: stat.color }">{{ stat.value }}</span>
            <span class="stat-label">{{ stat.label }}</span>
          </div>
        </div>
      </div>
    </section>

    <!-- Quick Actions -->
    <section class="section">
      <div class="section-header">
        <h2 class="section-title">快捷场景</h2>
      </div>
      <div class="scenes-grid">
        <button
          v-for="scene in scenes"
          :key="scene.id"
          class="scene-btn"
          :class="scene.id"
          @click="handleScenario(scene.id)"
        >
          <component :is="scene.icon" :size="22" />
          <span>{{ scene.label }}</span>
        </button>
      </div>
    </section>

    <!-- Device Grid -->
    <section class="section">
      <div class="section-header">
        <h2 class="section-title">设备控制</h2>
        <span class="section-count">{{ store.onlineDevices.length }} 台在线</span>
      </div>
      <div v-if="store.devices.length === 0" class="empty-state">
        <div class="empty-icon">
          <svg width="40" height="40" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1" opacity="0.3">
            <rect x="2" y="3" width="20" height="14" rx="2"/>
            <line x1="8" y1="21" x2="16" y2="21"/>
            <line x1="12" y1="17" x2="12" y2="21"/>
          </svg>
        </div>
        <p class="empty-text">等待设备连接...</p>
      </div>
      <div v-else class="device-grid">
        <div
          v-for="device in store.devices"
          :key="device.mac"
          class="device-card"
          @click="goToDevice(device.id)"
        >
          <div class="device-header">
            <div class="device-info">
              <h3 class="device-name">{{ device.name }}</h3>
              <span class="device-status" :class="{ online: device.online }">
                {{ device.online ? '在线' : '离线' }}
              </span>
            </div>
            <div class="device-toggle" @click.stop>
              <div class="device-actions">
                <button
                  class="action-btn"
                  title="全部打开"
                  @click.stop="broadcastCommand(device.id, 'all_on')"
                >
                  <Sun :size="16" />
                </button>
                <button
                  class="action-btn"
                  title="全部关闭"
                  @click.stop="broadcastCommand(device.id, 'all_off')"
                >
                  <Moon :size="16" />
                </button>
              </div>
            </div>
          </div>

          <div class="device-body">
            <div class="gpio-group">
              <div v-for="i in lightCount" :key="'l'+i" class="gpio-item">
                <button
                  class="gpio-btn light"
                  :class="{ active: device.gpios[i-1]?.value > 0 }"
                  @click.stop="toggleGpio(device.id, i-1)"
                >
                  <Lightbulb :size="16" />
                </button>
                <span class="gpio-label">{{ gpioNames(device.id, i-1) }}</span>
              </div>
            </div>

            <div class="gpio-row" v-if="hasRelays(device)">
              <div v-for="i in relayCount" :key="'r'+i" class="gpio-item">
                <button
                  class="gpio-btn relay"
                  :class="{ active: device.gpios[lightCount + i - 1]?.value > 0 }"
                  @click.stop="toggleGpio(device.id, lightCount + i - 1)"
                >
                  <Zap :size="16" />
                </button>
                <span class="gpio-label">{{ relayNames(device.id, i-1) }}</span>
              </div>
            </div>

            <div class="gpio-row" v-if="hasServos(device)">
              <div v-for="i in servoCount" :key="'s'+i" class="gpio-item">
                <div class="servo-control">
                  <input
                    type="range"
                    min="0"
                    max="180"
                    step="45"
                    :value="device.gpios[lightCount + relayCount + i - 1]?.value || 0"
                    @input="setServo(device.id, lightCount + relayCount + i - 1, Number(($event.target as HTMLInputElement).value))"
                    class="servo-slider"
                  />
                  <span class="gpio-label">{{ servoNames(device.id, i-1) }}</span>
                </div>
              </div>
            </div>
          </div>
        </div>
      </div>
    </section>

    <!-- Sensors & Events -->
    <section class="section">
      <div class="section-header">
        <h2 class="section-title">传感器</h2>
      </div>
      <div class="sensors-grid">
        <template v-if="hasSensorData">
          <div
            v-for="(sensor, idx) in sensorList"
            :key="idx"
            class="sensor-card"
            :class="sensor.status"
          >
            <component :is="sensor.icon" :size="18" />
            <div class="sensor-text">
              <span class="sensor-name">{{ sensor.name }}</span>
              <span class="sensor-value">{{ sensor.value }}</span>
            </div>
          </div>
        </template>
        <div v-else class="sensor-placeholder">
          <svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1" opacity="0.3">
            <path d="M12 2a10 10 0 100 20 10 10 0 000-20z"/>
            <path d="M12 6v6l4 2"/>
          </svg>
          <span>等待传感器数据</span>
        </div>
      </div>
    </section>

    <!-- Event Log -->
    <section class="section">
      <div class="section-header">
        <h2 class="section-title">事件日志</h2>
      </div>
      <div class="log-panel" ref="logPanel">
        <div v-if="store.logs.length === 0" class="log-empty">
          暂无事件记录
        </div>
        <div
          v-for="(log, idx) in store.logs.slice(0, 20)"
          :key="idx"
          class="log-item"
          :class="log.level"
        >
          <span class="log-time">{{ formatTime(log.timestamp) }}</span>
          <span class="log-msg">{{ log.message }}</span>
        </div>
      </div>
    </section>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue';
import { useRouter } from 'vue-router';
import { useDeviceStore } from '@/stores/deviceStore';
import { useWebSocket } from '@/composables/useWebSocket';
import { LIGHT_COUNT, RELAY_COUNT, SERVO_COUNT } from '@xiaozhi/shared';
import {
  Monitor, Wifi, Lightbulb, Zap, Activity,
  Sun, Moon, Home, LogOut, Cloud, Wind,
  Droplets, Thermometer, AlertTriangle
} from 'lucide-vue-next';

const router = useRouter();
const store = useDeviceStore();
const { sendCommand } = useWebSocket();

const lightCount = LIGHT_COUNT;
const relayCount = RELAY_COUNT;
const servoCount = SERVO_COUNT;

const stats = computed(() => [
  { label: '在线设备', value: store.onlineDevices.length, icon: Monitor, color: '#30d158', bg: 'rgba(48,209,88,0.1)' },
  { label: '总设备', value: store.devices.length, icon: Wifi, color: '#0071e3', bg: 'rgba(0,113,227,0.1)' },
  { label: '灯光开启', value: totalLightsOn.value, icon: Lightbulb, color: '#ff9f0a', bg: 'rgba(255,159,10,0.1)' },
  { label: '传感器', value: sensorAlertCount.value, icon: Activity, color: sensorAlertCount.value > 0 ? '#ff453a' : '#8e8e93', bg: sensorAlertCount.value > 0 ? 'rgba(255,69,58,0.1)' : 'rgba(142,142,147,0.1)' },
]);

const totalLightsOn = computed(() =>
  store.devices.reduce((s: number, d: any) =>
    s + d.gpios.slice(0, LIGHT_COUNT).filter((g: any) => g.value > 0).length, 0)
);

const sensorAlertCount = computed(() => {
  let n = 0;
  for (const d of store.sensorData || []) {
    if (d.sensors?.fire_status >= 2) n++;
    if (d.sensors?.help_status >= 1) n++;
  }
  return n;
});

const scenes = [
  { id: 'homecoming', label: '回家', icon: Home },
  { id: 'leavehome', label: '离家', icon: LogOut },
  { id: 'goodnight', label: '晚安', icon: Moon },
  { id: 'emergency', label: '紧急', icon: AlertTriangle },
  { id: 'rain', label: '收衣', icon: Cloud },
  { id: 'ventilate', label: '通风', icon: Wind },
];

function gpioNames(deviceId: string, index: number) {
  const names: Record<string, string[]> = {
    '1': ['大厅灯', '灯2', '灯3'],
    '2': ['主卧', '客厅', '厕所'],
    '3': ['阳台', '灯2', '灯3'],
  };
  return (names[deviceId] || [])[index] || `灯${index+1}`;
}

function relayNames(deviceId: string, index: number) {
  const names: Record<string, string[]> = {
    '1': ['继电器1', '继电器2', '继电器3'],
    '2': ['风扇', '继电器2', '继电器3'],
    '3': ['继电器1', '继电器2', '继电器3'],
  };
  return (names[deviceId] || [])[index] || `继电器${index+1}`;
}

function servoNames(deviceId: string, index: number) {
  const names: Record<string, string[]> = {
    '1': ['大门', '舵机2', '舵机3'],
    '2': ['晾衣架', '舵机2', '舵机3'],
    '3': ['左天窗', '右天窗', '晾衣架'],
  };
  return (names[deviceId] || [])[index] || `舵机${index+1}`;
}

function hasRelays(device: any) { return device.gpios.length > LIGHT_COUNT; }
function hasServos(device: any) { return device.gpios.length > LIGHT_COUNT + RELAY_COUNT; }

const hasSensorData = computed(() => store.sensorData?.length > 0);

const sensorList = computed(() => {
  const list: any[] = [];
  for (const dev of store.sensorData || []) {
    const s = dev.sensors;
    if (!s || !s.updated_ms) continue;
    if (s.smoke_mv > 0) {
      list.push({ name: `${dev.name} 烟雾`, icon: Thermometer, value: s.fire_status >= 2 ? '火灾!' : `${s.smoke_mv}mV`, status: s.fire_status >= 2 ? 'alert' : 'normal' });
    }
    if (s.rain_mv > 0) {
      list.push({ name: `${dev.name} 雨滴`, icon: Droplets, value: s.rain_status >= 1 ? '下雨中' : `${s.rain_mv}mV`, status: s.rain_status >= 1 ? 'warning' : 'normal' });
    }
    if (s.help_status > 0) {
      list.push({ name: `${dev.name} 求助`, icon: AlertTriangle, value: '已触发', status: 'alert' });
    }
  }
  return list;
});

function toggleGpio(deviceId: string, gpioIndex: number) {
  const dev = store.devices.find(d => d.id === deviceId);
  const current = dev?.gpios[gpioIndex]?.value || 0;
  sendCommand({ deviceId, command: 'set_gpio', gpioIndex, value: current ? 0 : 1 });
}

function setServo(deviceId: string, gpioIndex: number, value: number) {
  sendCommand({ deviceId, command: 'set_servo', gpioIndex, value });
}

function broadcastCommand(deviceId: string, action: string) {
  if (action === 'all_on') {
    for (let i = 0; i < LIGHT_COUNT; i++) {
      sendCommand({ deviceId, command: 'set_gpio', gpioIndex: i, value: 1 });
    }
  } else {
    for (let i = 0; i < LIGHT_COUNT; i++) {
      sendCommand({ deviceId, command: 'set_gpio', gpioIndex: i, value: 0 });
    }
  }
}

function handleScenario(scenario: string) {
  sendCommand({ deviceId: 'broadcast', command: 'scenario', scenario });
}

function goToDevice(id: string) {
  router.push(`/device/${id}`);
}

function formatTime(ts: number) {
  const d = new Date(ts);
  return `${String(d.getHours()).padStart(2,'0')}:${String(d.getMinutes()).padStart(2,'0')}:${String(d.getSeconds()).padStart(2,'0')}`;
}
</script>

<style scoped>
.home-container {
  display: flex;
  flex-direction: column;
  gap: 32px;
}

.hero-section {
  text-align: center;
}

.hero-header {
  margin-bottom: 28px;
}

.hero-title {
  font-size: 40px;
  font-weight: 700;
  letter-spacing: -0.03em;
  line-height: 1.1;
  background: linear-gradient(135deg, #1d1d1f, #434344);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  background-clip: text;
  margin: 0 0 6px;
}

.hero-subtitle {
  font-size: 17px;
  color: #86868b;
  margin: 0;
  font-weight: 400;
}

.stats-grid {
  display: grid;
  grid-template-columns: repeat(4, 1fr);
  gap: 12px;
}

.stat-card {
  display: flex;
  align-items: center;
  gap: 14px;
  background: white;
  border-radius: 16px;
  padding: 18px;
  border: 1px solid rgba(0,0,0,0.06);
  transition: transform 0.2s, box-shadow 0.2s;
}

.stat-card:hover {
  transform: translateY(-2px);
  box-shadow: 0 8px 24px rgba(0,0,0,0.06);
}

.stat-icon {
  width: 40px;
  height: 40px;
  border-radius: 12px;
  display: flex;
  align-items: center;
  justify-content: center;
  color: #1d1d1f;
  flex-shrink: 0;
}

.stat-info {
  display: flex;
  flex-direction: column;
}

.stat-value {
  font-size: 22px;
  font-weight: 700;
  letter-spacing: -0.02em;
  line-height: 1.2;
}

.stat-label {
  font-size: 12px;
  color: #86868b;
  font-weight: 500;
}

/* Sections */
.section-header {
  display: flex;
  align-items: baseline;
  justify-content: space-between;
  margin-bottom: 16px;
}

.section-title {
  font-size: 24px;
  font-weight: 700;
  letter-spacing: -0.02em;
  margin: 0;
}

.section-count {
  font-size: 13px;
  color: #86868b;
  font-weight: 500;
}

/* Scenes */
.scenes-grid {
  display: grid;
  grid-template-columns: repeat(6, 1fr);
  gap: 10px;
}

.scene-btn {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 6px;
  padding: 16px 8px;
  border-radius: 14px;
  border: 1px solid rgba(0,0,0,0.06);
  background: white;
  color: #1d1d1f;
  font-size: 13px;
  font-weight: 500;
  cursor: pointer;
  transition: all 0.2s;
}
.scene-btn:hover { transform: translateY(-2px); box-shadow: 0 8px 24px rgba(0,0,0,0.08); }
.scene-btn:active { transform: scale(0.97); }
.scene-btn.homecoming { color: #ff9f0a; }
.scene-btn.leavehome { color: #0071e3; }
.scene-btn.goodnight { color: #af52de; }
.scene-btn.emergency { color: #ff453a; }
.scene-btn.rain { color: #0071e3; }
.scene-btn.ventilate { color: #30d158; }

/* Device Grid */
.device-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(340px, 1fr));
  gap: 14px;
}

.device-card {
  background: white;
  border-radius: 18px;
  border: 1px solid rgba(0,0,0,0.06);
  padding: 20px;
  cursor: pointer;
  transition: all 0.2s;
}

.device-card:hover {
  transform: translateY(-2px);
  box-shadow: 0 12px 32px rgba(0,0,0,0.08);
  border-color: rgba(0,113,227,0.2);
}

.device-header {
  display: flex;
  justify-content: space-between;
  align-items: flex-start;
  margin-bottom: 16px;
}

.device-info {
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.device-name {
  font-size: 17px;
  font-weight: 600;
  margin: 0;
  letter-spacing: -0.02em;
}

.device-status {
  font-size: 12px;
  color: #8e8e93;
  font-weight: 500;
}

.device-status.online {
  color: #30d158;
}

.device-actions {
  display: flex;
  gap: 6px;
}

.action-btn {
  width: 32px;
  height: 32px;
  border-radius: 8px;
  border: 1px solid rgba(0,0,0,0.06);
  background: #f5f5f7;
  cursor: pointer;
  display: flex;
  align-items: center;
  justify-content: center;
  color: #86868b;
  transition: all 0.15s;
}

.action-btn:hover {
  background: #e8e8ed;
  color: #1d1d1f;
}

.device-body {
  display: flex;
  flex-direction: column;
  gap: 10px;
}

.gpio-group, .gpio-row {
  display: flex;
  gap: 8px;
  flex-wrap: wrap;
}

.gpio-item {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 4px;
}

.gpio-btn {
  width: 40px;
  height: 40px;
  border-radius: 10px;
  border: 1px solid rgba(0,0,0,0.06);
  background: #f5f5f7;
  cursor: pointer;
  display: flex;
  align-items: center;
  justify-content: center;
  color: #86868b;
  transition: all 0.15s;
}

.gpio-btn:hover { background: #e8e8ed; }

.gpio-btn.light.active {
  background: #0071e3;
  color: white;
  border-color: #0071e3;
  box-shadow: 0 4px 12px rgba(0,113,227,0.3);
}

.gpio-btn.relay.active {
  background: #30d158;
  color: white;
  border-color: #30d158;
  box-shadow: 0 4px 12px rgba(48,209,88,0.3);
}

.gpio-label {
  font-size: 10px;
  color: #86868b;
  font-weight: 500;
  white-space: nowrap;
}

.servo-control {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 4px;
}

.servo-slider {
  -webkit-appearance: none;
  width: 60px;
  height: 3px;
  border-radius: 2px;
  background: #e8e8ed;
  outline: none;
}

.servo-slider::-webkit-slider-thumb {
  -webkit-appearance: none;
  width: 16px;
  height: 16px;
  border-radius: 50%;
  background: white;
  border: 1px solid rgba(0,0,0,0.1);
  box-shadow: 0 2px 6px rgba(0,0,0,0.1);
  cursor: pointer;
}

/* Empty state */
.empty-state {
  display: flex;
  flex-direction: column;
  align-items: center;
  padding: 60px 20px;
  background: white;
  border-radius: 18px;
  border: 1px solid rgba(0,0,0,0.06);
}

.empty-icon {
  margin-bottom: 12px;
  opacity: 0.5;
}

.empty-text {
  font-size: 15px;
  color: #86868b;
  margin: 0;
}

/* Sensors */
.sensors-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(160px, 1fr));
  gap: 10px;
}

.sensor-card {
  display: flex;
  align-items: center;
  gap: 10px;
  background: white;
  border-radius: 14px;
  padding: 14px;
  border: 1px solid rgba(0,0,0,0.06);
}

.sensor-card.alert {
  background: rgba(255,69,58,0.06);
  border-color: rgba(255,69,58,0.2);
  color: #ff453a;
}

.sensor-card.warning {
  background: rgba(255,159,10,0.06);
  border-color: rgba(255,159,10,0.2);
  color: #ff9f0a;
}

.sensor-text {
  display: flex;
  flex-direction: column;
  gap: 1px;
}

.sensor-name {
  font-size: 12px;
  color: #86868b;
  font-weight: 500;
}

.sensor-value {
  font-size: 14px;
  font-weight: 600;
}

.sensor-placeholder {
  display: flex;
  align-items: center;
  gap: 8px;
  color: #86868b;
  font-size: 13px;
  background: white;
  border-radius: 14px;
  padding: 16px;
  border: 1px solid rgba(0,0,0,0.06);
}

/* Log */
.log-panel {
  background: white;
  border-radius: 18px;
  border: 1px solid rgba(0,0,0,0.06);
  padding: 12px;
  max-height: 200px;
  overflow-y: auto;
}

.log-empty {
  text-align: center;
  padding: 20px;
  color: #86868b;
  font-size: 13px;
}

.log-item {
  display: flex;
  gap: 10px;
  padding: 6px 10px;
  border-radius: 6px;
  font-size: 12px;
  font-family: "SF Mono", SFMono-Regular, monospace;
}

.log-item.info { color: #1d1d1f; }
.log-item.warn { color: #ff9f0a; }
.log-item.error { color: #ff453a; }

.log-time {
  color: #86868b;
  flex-shrink: 0;
}

@media (max-width: 768px) {
  .stats-grid { grid-template-columns: repeat(2, 1fr); }
  .scenes-grid { grid-template-columns: repeat(3, 1fr); }
  .hero-title { font-size: 28px; }
  .device-grid { grid-template-columns: 1fr; }
  .sensors-grid { grid-template-columns: repeat(2, 1fr); }
}
</style>
