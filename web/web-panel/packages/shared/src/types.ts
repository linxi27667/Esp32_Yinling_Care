export interface GpioState {
  index: number;
  name: string;
  value: number;
  mode: 'output' | 'input';
}

export interface DeviceState {
  id: string;
  name: string;
  mac: string;
  online: boolean;
  gpioCount: number;
  gpios: GpioState[];
  lastHeartbeat: number;
}

export interface LocalGpioState {
  name: string;
  pin: number;
  value: number;
}

export interface WsCommandPayload {
  deviceId: string;
  command: 'set_gpio' | 'set_light' | 'set_relay' | 'set_servo' | 'scenario';
  gpioIndex?: number;
  value?: number;
  scenario?: 'homecoming' | 'leavehome' | 'goodnight' | 'emergency' | 'rain' | 'ventilate';
}

export interface WsMessage {
  type: 'command' | 'state_update' | 'device_response' | 'log' | 'heartbeat';
  payload: WsCommandPayload | StateUpdatePayload | DeviceResponsePayload | LogPayload | HeartbeatPayload;
}

export interface StateUpdatePayload {
  devices: DeviceState[];
  localGpios: LocalGpioState[];
  sensorData?: DeviceSensorData[];
  timestamp: number;
}

export interface DeviceResponsePayload {
  deviceId: string;
  mac: string;
  command: number;
  gpioIndex: number;
  value: number;
  success: boolean;
}

export interface LogPayload {
  level: 'info' | 'warn' | 'error';
  message: string;
  timestamp: number;
}

export interface HeartbeatPayload {
  mac: string;
  deviceId: string;
  timestamp: number;
}

export interface IotCommandPacket {
  command: number;
  deviceId: number;
  gpioIndex: number;
  value: number;
  reserved: Buffer;
}

export interface IotAnnounceV2Packet {
  command: number;
  deviceId: number;
  gpioIndex: number;
  value: number;
  deviceName: string;
}

export interface SensorState {
  smoke_mv: number;
  rain_mv: number;
  fire_status: number;
  rain_status: number;
  help_status: number;
  updated_ms: number;
}

export interface DeviceSensorData {
  name: string;
  online: boolean;
  sensors: SensorState;
}

export interface EventLogEntry {
  idx: number;
  msg: string;
}

export interface SensorUpdatePayload {
  devices: DeviceSensorData[];
  timestamp: number;
}

export interface EventLogPayload {
  events: EventLogEntry[];
  timestamp: number;
}
