import { defineStore } from 'pinia';
import { ref, computed } from 'vue';
import type { DeviceState, GpioState, LocalGpioState, DeviceSensorData } from '@xiaozhi/shared';

export const useDeviceStore = defineStore('device', () => {
  const devices = ref<DeviceState[]>([]);
  const localGpios = ref<LocalGpioState[]>([]);
  const sensorData = ref<DeviceSensorData[]>([]);
  const logs = ref<{ level: 'info' | 'warn' | 'error'; message: string; timestamp: number }[]>([]);
  const pendingCommands = ref<Set<string>>(new Set());

  const onlineDevices = computed(() => devices.value.filter((d) => d.online));
  const offlineDevices = computed(() => devices.value.filter((d) => !d.online));

  function updateDevices(newDevices: DeviceState[]) {
    devices.value = newDevices;
  }

  function updateLocalGpios(newGpios: LocalGpioState[]) {
    localGpios.value = newGpios;
  }

  function updateSensorData(data: DeviceSensorData[]) {
    sensorData.value = data;
  }

  function addLog(level: 'info' | 'warn' | 'error', message: string) {
    logs.value.unshift({ level, message, timestamp: Date.now() });
    if (logs.value.length > 200) {
      logs.value = logs.value.slice(0, 200);
    }
  }

  function setPending(key: string) {
    pendingCommands.value.add(key);
  }

  function clearPending(key: string) {
    pendingCommands.value.delete(key);
  }

  function isPending(deviceId: string, gpioIndex: number) {
    return pendingCommands.value.has(`${deviceId}-${gpioIndex}`);
  }

  return {
    devices,
    localGpios,
    logs,
    onlineDevices,
    offlineDevices,
    pendingCommands,
    sensorData,
    updateDevices,
    updateLocalGpios,
    updateSensorData,
    addLog,
    setPending,
    clearPending,
    isPending,
  };
});
