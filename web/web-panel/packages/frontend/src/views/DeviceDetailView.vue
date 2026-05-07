<template>
  <div class="detail-container">
    <button class="back-btn" @click="router.push('/')">
      <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
        <polyline points="15 18 9 12 15 6"/>
      </svg>
      返回
    </button>

    <div v-if="device" class="detail-content">
      <div class="detail-hero">
        <h1 class="detail-title">{{ device.name }}</h1>
        <span class="detail-status" :class="{ online: device.online }">
          {{ device.online ? '在线' : '离线' }}
        </span>
      </div>

      <div class="controls-section">
        <h2 class="control-heading">灯光</h2>
        <div class="control-grid">
          <div
            v-for="i in lightCount"
            :key="'l'+i"
            class="control-item"
            :class="{ active: device.gpios[i-1]?.value > 0 }"
            @click="toggle(i-1)"
          >
            <Lightbulb :size="24" />
            <span class="control-name">{{ gpioNames(device.id, i-1) }}</span>
          </div>
        </div>

        <h2 class="control-heading" v-if="hasRelays">继电器</h2>
        <div class="control-grid" v-if="hasRelays">
          <div
            v-for="i in relayCount"
            :key="'r'+i"
            class="control-item relay"
            :class="{ active: device.gpios[lightCount + i - 1]?.value > 0 }"
            @click="toggle(lightCount + i - 1)"
          >
            <Zap :size="24" />
            <span class="control-name">{{ relayNames(device.id, i-1) }}</span>
          </div>
        </div>

        <h2 class="control-heading" v-if="hasRelays">全部操作</h2>
        <div class="action-row">
          <button class="action-button on" @click="allOn">
            <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="5"/><line x1="12" y1="1" x2="12" y2="5"/><line x1="12" y1="19" x2="12" y2="23"/></svg>
            全部打开
          </button>
          <button class="action-button off" @click="allOff">
            <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="5"/><line x1="4.93" y1="4.93" x2="7.76" y2="7.76"/><line x1="16.24" y1="16.24" x2="19.07" y2="19.07"/></svg>
            全部关闭
          </button>
        </div>

        <h2 class="control-heading" v-if="hasServos">舵机</h2>
        <div class="servo-grid" v-if="hasServos">
          <div v-for="i in servoCount" :key="'s'+i" class="servo-item">
            <span class="servo-label">{{ servoNames(device.id, i-1) }}</span>
            <div class="servo-value-row">
              <span class="servo-angle">{{ device.gpios[lightCount + relayCount + i - 1]?.value || 0 }}°</span>
              <input
                type="range" min="0" max="180" step="45"
                class="servo-thumb"
                :value="device.gpios[lightCount + relayCount + i - 1]?.value || 0"
                @input="setServo(lightCount + relayCount + i - 1, Number(($event.target as HTMLInputElement).value))"
              />
            </div>
            <div class="servo-presets">
              <button v-for="a in [0, 45, 90, 135, 180]" :key="a"
                class="preset-btn"
                :class="{ active: (device.gpios[lightCount + relayCount + i - 1]?.value || 0) === a }"
                @click="setServo(lightCount + relayCount + i - 1, a)"
              >{{ a }}°</button>
            </div>
          </div>
        </div>
      </div>

      <div class="mac-display">
        <span class="mac-label">MAC</span>
        <span class="mac-value">{{ device.mac }}</span>
      </div>
    </div>

    <div v-else class="not-found">
      <p>设备未找到</p>
      <button class="back-btn centered" @click="router.push('/')">返回首页</button>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue';
import { useRoute, useRouter } from 'vue-router';
import { useDeviceStore } from '@/stores/deviceStore';
import { useWebSocket } from '@/composables/useWebSocket';
import { LIGHT_COUNT, RELAY_COUNT, SERVO_COUNT } from '@xiaozhi/shared';
import { Lightbulb, Zap } from 'lucide-vue-next';

const route = useRoute();
const router = useRouter();
const store = useDeviceStore();
const { sendCommand } = useWebSocket();

const lightCount = LIGHT_COUNT;
const relayCount = RELAY_COUNT;
const servoCount = SERVO_COUNT;

const deviceId = computed(() => route.params.deviceId as string);
const device = computed(() => store.devices.find(d => d.id === deviceId.value) || null);
const hasRelays = computed(() => device.value && device.value.gpios.length > LIGHT_COUNT);
const hasServos = computed(() => device.value && device.value.gpios.length > LIGHT_COUNT + RELAY_COUNT);

function gpioNames(id: string, index: number) {
  const m: Record<string, string[]> = { '1': ['大厅灯','灯2','灯3'], '2': ['主卧','客厅','厕所'], '3': ['阳台','灯2','灯3'] };
  return (m[id] || [])[index] || `灯${index+1}`;
}
function relayNames(id: string, index: number) {
  const m: Record<string, string[]> = { '1': ['继电器1','继电器2','继电器3'], '2': ['风扇','继电器2','继电器3'], '3': ['继电器1','继电器2','继电器3'] };
  return (m[id] || [])[index] || `继电器${index+1}`;
}
function servoNames(id: string, index: number) {
  const m: Record<string, string[]> = { '1': ['大门','舵机2','舵机3'], '2': ['晾衣架','舵机2','舵机3'], '3': ['左天窗','右天窗','晾衣架'] };
  return (m[id] || [])[index] || `舵机${index+1}`;
}

function toggle(i: number) {
  sendCommand({ deviceId: deviceId.value, command: 'set_gpio', gpioIndex: i, value: device.value?.gpios[i]?.value ? 0 : 1 });
}
function setServo(i: number, v: number) {
  sendCommand({ deviceId: deviceId.value, command: 'set_servo', gpioIndex: i, value: v });
}
function allOn() {
  for (let i = 0; i < lightCount; i++) sendCommand({ deviceId: deviceId.value, command: 'set_gpio', gpioIndex: i, value: 1 });
}
function allOff() {
  for (let i = 0; i < lightCount; i++) sendCommand({ deviceId: deviceId.value, command: 'set_gpio', gpioIndex: i, value: 0 });
}
</script>

<style scoped>
.detail-container { animation: slideUp .5s ease-out both; }
@keyframes slideUp { from { opacity: 0; transform: translateY(16px); } to { opacity: 1; transform: translateY(0); }}

.back-btn {
  display: inline-flex; align-items: center; gap: 4px; padding: 6px 14px;
  border: 1px solid rgba(0,0,0,.06); background: white; border-radius: 20px;
  font-size: 14px; font-weight: 500; cursor: pointer; margin-bottom: 20px;
}
.back-btn:hover { background: #f5f5f7; }
.back-btn.centered { display: flex; margin: 20px auto; }

.detail-hero { display: flex; align-items: baseline; gap: 12px; margin-bottom: 28px; }
.detail-title { font-size: 32px; font-weight: 700; letter-spacing: -.03em; margin: 0; }
.detail-status { font-size: 12px; font-weight: 600; padding: 3px 12px; border-radius: 20px; }
.detail-status.online { background: rgba(48,209,88,.12); color: #30d158; }
.detail-status:not(.online) { background: rgba(142,142,147,.12); color: #8e8e93; }

.controls-section { margin-bottom: 24px; }
.control-heading { font-size: 17px; font-weight: 600; margin: 0 0 12px; letter-spacing: -.02em; }
.control-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(100px, 1fr)); gap: 10px; margin-bottom: 20px; }
.control-item {
  display: flex; flex-direction: column; align-items: center; gap: 8px;
  padding: 20px 12px; background: white; border: 1px solid rgba(0,0,0,.06);
  border-radius: 14px; cursor: pointer; color: #86868b; transition: all .2s;
}
.control-item:hover { border-color: rgba(0,113,227,.3); }
.control-item.active { background: #0071e3; border-color: #0071e3; color: white; box-shadow: 0 8px 24px rgba(0,113,227,.3); }
.control-item.relay.active { background: #30d158; border-color: #30d158; box-shadow: 0 8px 24px rgba(48,209,88,.3); }
.control-name { font-size: 12px; font-weight: 500; }

.action-row { display: flex; gap: 10px; margin-bottom: 24px; }
.action-button {
  flex: 1; display: flex; align-items: center; justify-content: center; gap: 8px;
  padding: 12px; border-radius: 12px; border: 1px solid rgba(0,0,0,.06);
  font-size: 14px; font-weight: 600; cursor: pointer; transition: all .15s;
}
.action-button.on { background: rgba(0,113,227,.08); color: #0071e3; border-color: rgba(0,113,227,.2); }
.action-button.off { background: rgba(255,69,58,.08); color: #ff453a; border-color: rgba(255,69,58,.2); }
.action-button:hover { transform: translateY(-1px); }

.servo-grid { display: flex; flex-direction: column; gap: 14px; margin-bottom: 24px; }
.servo-item { background: white; border: 1px solid rgba(0,0,0,.06); border-radius: 14px; padding: 16px; }
.servo-label { font-size: 14px; font-weight: 600; display: block; margin-bottom: 10px; }
.servo-value-row { display: flex; align-items: center; gap: 14px; margin-bottom: 10px; }
.servo-angle { font-size: 15px; font-weight: 700; min-width: 36px; color: #0071e3; font-variant-numeric: tabular-nums; }
.servo-thumb { flex: 1; -webkit-appearance: none; height: 4px; border-radius: 2px; background: #e8e8ed; }
.servo-thumb::-webkit-slider-thumb { -webkit-appearance: none; width: 20px; height: 20px; border-radius: 50%; background: white; border: 1px solid rgba(0,0,0,.08); box-shadow: 0 2px 8px rgba(0,0,0,.08); cursor: pointer; }
.servo-presets { display: flex; gap: 6px; }
.preset-btn { padding: 4px 10px; border: 1px solid rgba(0,0,0,.06); background: #f5f5f7; border-radius: 8px; font-size: 11px; font-weight: 600; color: #86868b; cursor: pointer; }
.preset-btn.active { background: #0071e3; color: white; border-color: #0071e3; }
.mac-display { display: flex; gap: 10px; padding: 12px 16px; background: #f5f5f7; border-radius: 10px; font-size: 12px; font-family: "SF Mono",monospace; }
.mac-label { color: #86868b; } .mac-value { color: #1d1d1f; }
.not-found { text-align: center; padding: 60px; color: #86868b; }
</style>
