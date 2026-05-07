export const MQTT_TOPIC_CMD_BROADCAST = 'xiaozhi/iot/cmd/broadcast';
export const MQTT_TOPIC_CMD_PREFIX = 'xiaozhi/iot/cmd/';
export const MQTT_TOPIC_RESP_PREFIX = 'xiaozhi/iot/resp/';
export const MQTT_TOPIC_HEARTBEAT_PREFIX = 'xiaozhi/iot/heartbeat/';
export const MQTT_TOPIC_ANNOUNCE_PREFIX = 'xiaozhi/iot/announce/';
export const MQTT_TOPIC_SENSOR_PREFIX = 'xiaozhi/iot/sensor/';

export const IOT_CMD_SET_GPIO = 0x01;
export const IOT_CMD_GET_GPIO = 0x02;
export const IOT_CMD_GET_ALL_GPIO = 0x03;
export const IOT_CMD_HEARTBEAT = 0x04;
export const IOT_CMD_RESPONSE = 0x05;
export const IOT_CMD_DISCOVER = 0x06;
export const IOT_CMD_ANNOUNCE = 0x07;
export const IOT_CMD_ANNOUNCE_V2 = 0x08;
export const IOT_CMD_SET_SERVO = 0x10;
export const IOT_CMD_SET_LIGHT = 0x11;
export const IOT_CMD_SET_RELAY = 0x12;
export const IOT_CMD_SENSOR_REPORT = 0x13;
export const IOT_CMD_BROADCAST_ALL_OFF = 0x30;
export const IOT_CMD_BROADCAST_ALL_ON = 0x31;
export const IOT_CMD_BROADCAST_LIGHTS_OFF = 0x32;
export const IOT_CMD_BROADCAST_LIGHTS_ON = 0x33;
export const IOT_CMD_EMERGENCY = 0x34;

export const DEVICE_ID_FIRSTFLOOR = '1';
export const DEVICE_ID_SECONDFLOOR = '2';
export const DEVICE_ID_THIRDFLOOR = '3';

export const DEVICE_NAMES: Record<string, string> = {
  [DEVICE_ID_FIRSTFLOOR]: '一楼设备',
  [DEVICE_ID_SECONDFLOOR]: '二楼设备',
  [DEVICE_ID_THIRDFLOOR]: '三楼设备',
};

export const GPIO_NAMES: Record<string, string[]> = {
  [DEVICE_ID_FIRSTFLOOR]: ['大厅灯', '灯2', '灯3', '继电器1', '继电器2', '继电器3', '大门', '舵机2', '舵机3'],
  [DEVICE_ID_SECONDFLOOR]: ['主卧灯', '客厅灯', '厕所灯', '风扇', '继电器2', '继电器3', '晾衣架', '舵机2', '舵机3'],
  [DEVICE_ID_THIRDFLOOR]: ['阳台灯', '灯2', '灯3', '继电器1', '继电器2', '继电器3', '左天窗', '右天窗', '晾衣架'],
};

export const SENSOR_TYPE_NAMES: Record<number, string> = {
  0: '烟雾ADC(mV)',
  1: '雨滴ADC(mV)',
  2: '火灾状态',
  3: '下雨状态',
  4: '求助状态',
};

export const LIGHT_COUNT = 3;
export const RELAY_COUNT = 3;
export const SERVO_COUNT = 3;

export const DEVICE_TIMEOUT_MS = 60000;
export const HEARTBEAT_INTERVAL_MS = 30000;
