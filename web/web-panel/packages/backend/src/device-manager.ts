import type { DeviceState, GpioState, DeviceSensorData, SensorState } from '@xiaozhi/shared';
import {
  DEVICE_NAMES,
  GPIO_NAMES,
  LIGHT_COUNT,
  RELAY_COUNT,
  SERVO_COUNT,
  DEVICE_TIMEOUT_MS,
} from '@xiaozhi/shared';

export class DeviceManager {
  private devices: Map<string, DeviceState> = new Map();
  private localGpios: GpioState[] = [];
  private sensorData: Map<string, SensorState> = new Map();
  private listeners: Set<() => void> = new Set();

  constructor() {
    this.initLocalGpios();
    this.startTimeoutChecker();
  }

  private initLocalGpios() {
    this.localGpios = [
      { index: 0, name: 'led1', value: 0, mode: 'output' },
      { index: 1, name: 'led2', value: 0, mode: 'output' },
      { index: 2, name: 'relay1', value: 0, mode: 'output' },
      { index: 3, name: 'relay2', value: 0, mode: 'output' },
    ];
  }

  private startTimeoutChecker() {
    setInterval(() => {
      const now = Date.now();
      let changed = false;
      for (const device of this.devices.values()) {
        if (device.online && now - device.lastHeartbeat > DEVICE_TIMEOUT_MS) {
          device.online = false;
          changed = true;
        }
      }
      if (changed) this.notify();
    }, 10000);
  }

  onChange(listener: () => void) {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  private notify() {
    for (const listener of this.listeners) {
      try { listener(); } catch {}
    }
  }

  getDevices(): DeviceState[] {
    return Array.from(this.devices.values()).sort((a, b) => a.id.localeCompare(b.id));
  }

  getLocalGpios(): GpioState[] {
    return this.localGpios;
  }

  addOrUpdateDevice(mac: string, deviceId: string, name: string, gpioCount: number): DeviceState {
    const existing = this.devices.get(mac);
    const gpios: GpioState[] = [];
    const names = GPIO_NAMES[deviceId] || [];
    for (let i = 0; i < gpioCount; i++) {
      gpios.push({
        index: i,
        name: names[i] || `gpio${i}`,
        value: existing?.gpios[i]?.value ?? 0,
        mode: 'output',
      });
    }

    const device: DeviceState = {
      id: deviceId,
      name: name || DEVICE_NAMES[deviceId] || `设备${deviceId}`,
      mac,
      online: true,
      gpioCount: gpios.length,
      gpios,
      lastHeartbeat: Date.now(),
    };

    this.devices.set(mac, device);
    this.notify();
    return device;
  }

  updateHeartbeat(mac: string) {
    const device = this.devices.get(mac);
    if (device) {
      device.online = true;
      device.lastHeartbeat = Date.now();
      this.notify();
    }
  }

  updateGpioState(mac: string, gpioIndex: number, value: number) {
    const device = this.devices.get(mac);
    if (device && gpioIndex >= 0 && gpioIndex < device.gpios.length) {
      device.gpios[gpioIndex].value = value;
      this.notify();
    }
  }

  setLocalGpio(index: number, value: number) {
    if (index >= 0 && index < this.localGpios.length) {
      this.localGpios[index].value = value;
      this.notify();
    }
  }

  getDeviceById(deviceId: string): DeviceState | undefined {
    for (const device of this.devices.values()) {
      if (device.id === deviceId) return device;
    }
    return undefined;
  }

  getAllState() {
    return {
      devices: this.getDevices(),
      localGpios: this.getLocalGpios(),
      timestamp: Date.now(),
    };
  }

  updateSensorData(mac: string, deviceId: string, sensorType: number, value: number) {
    let s = this.sensorData.get(mac);
    if (!s) {
      s = { smoke_mv: 0, rain_mv: 0, fire_status: 0, rain_status: 0, help_status: 0, updated_ms: 0 };
      this.sensorData.set(mac, s);
    }
    s.updated_ms = Date.now();
    switch (sensorType) {
      case 0: s.smoke_mv = value; break;
      case 1: s.rain_mv = value; break;
      case 2: s.fire_status = value; break;
      case 3: s.rain_status = value; break;
      case 4: s.help_status = value; break;
    }
    // Also add/update device entry so it shows up
    const existing = this.devices.get(mac);
    if (!existing) {
      const name = DEVICE_NAMES[deviceId] || `设备${deviceId}`;
      this.addOrUpdateDevice(mac, deviceId, name, 9);
    }
    this.notify();
  }

  getSensorData(): DeviceSensorData[] {
    const result: DeviceSensorData[] = [];
    for (const [mac, sensors] of this.sensorData) {
      const dev = this.devices.get(mac);
      result.push({
        name: dev?.name || '未知设备',
        online: dev?.online || false,
        sensors,
      });
    }
    return result;
  }
}
