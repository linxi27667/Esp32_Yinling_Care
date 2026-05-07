import mqtt from 'mqtt';
import type { DeviceManager } from './device-manager';
import {
  parseMacFromTopic,
  parseCommandPacket,
  parseAnnounceV2Packet,
} from './utils';
import {
  MQTT_TOPIC_CMD_BROADCAST,
  MQTT_TOPIC_CMD_PREFIX,
  MQTT_TOPIC_RESP_PREFIX,
  MQTT_TOPIC_HEARTBEAT_PREFIX,
  MQTT_TOPIC_ANNOUNCE_PREFIX,
  MQTT_TOPIC_SENSOR_PREFIX,
  IOT_CMD_HEARTBEAT,
  IOT_CMD_RESPONSE,
  IOT_CMD_ANNOUNCE,
  IOT_CMD_ANNOUNCE_V2,
  IOT_CMD_SENSOR_REPORT,
} from '@xiaozhi/shared';

export class MqttClient {
  private client?: mqtt.MqttClient;
  private deviceManager: DeviceManager;
  private onLog: (level: 'info' | 'warn' | 'error', message: string) => void;

  constructor(
    deviceManager: DeviceManager,
    onLog: (level: 'info' | 'warn' | 'error', message: string) => void
  ) {
    this.deviceManager = deviceManager;
    this.onLog = onLog;
  }

  async connect(brokerUrl: string = 'mqtt://127.0.0.1:1883') {
    return new Promise<void>((resolve, reject) => {
      this.client = mqtt.connect(brokerUrl, {
        clientId: `xiaozhi_web_panel_${Date.now()}`,
        reconnectPeriod: 5000,
      });

      this.client.on('connect', async () => {
        this.onLog('info', `MQTT connected to ${brokerUrl}`);

        await this.client!.subscribe([
          `${MQTT_TOPIC_RESP_PREFIX}+`,
          `${MQTT_TOPIC_HEARTBEAT_PREFIX}+`,
          `${MQTT_TOPIC_ANNOUNCE_PREFIX}+`,
          `${MQTT_TOPIC_SENSOR_PREFIX}+`,
        ]);

        this.onLog('info', 'MQTT subscribed to resp/+, heartbeat/+, announce/+, sensor/+');
        resolve();
      });

      this.client.on('message', (topic, message) => {
        this.handleMessage(topic, message);
      });

      this.client.on('error', (err) => {
        this.onLog('error', `MQTT error: ${err.message}`);
        reject(err);
      });

      this.client.on('close', () => {
        this.onLog('warn', 'MQTT connection closed');
      });
    });
  }

  private handleMessage(topic: string, message: Buffer) {
    const mac =
      parseMacFromTopic(topic, MQTT_TOPIC_RESP_PREFIX) ||
      parseMacFromTopic(topic, MQTT_TOPIC_HEARTBEAT_PREFIX) ||
      parseMacFromTopic(topic, MQTT_TOPIC_ANNOUNCE_PREFIX) ||
      parseMacFromTopic(topic, MQTT_TOPIC_SENSOR_PREFIX);

    if (!mac) return;

    const packet = parseCommandPacket(message);
    if (!packet) return;

    switch (packet.command) {
      case IOT_CMD_HEARTBEAT: {
        this.deviceManager.updateHeartbeat(mac);
        this.onLog('info', `Heartbeat from ${mac}`);
        break;
      }
      case IOT_CMD_RESPONSE: {
        this.deviceManager.updateGpioState(mac, packet.gpioIndex, packet.value);
        this.onLog('info', `Response from ${mac}: gpio=${packet.gpioIndex}, value=${packet.value}`);
        break;
      }
      case IOT_CMD_ANNOUNCE: {
        const gpioCount = packet.gpioIndex > 0 ? packet.gpioIndex : 4;
        this.deviceManager.addOrUpdateDevice(mac, String(packet.deviceId), `设备${packet.deviceId}`, gpioCount);
        this.onLog('info', `Announce from ${mac}: deviceId=${packet.deviceId}, gpios=${gpioCount}`);
        break;
      }
      case IOT_CMD_ANNOUNCE_V2: {
        const announce = parseAnnounceV2Packet(message);
        if (announce) {
          const gpioCount = announce.gpioIndex > 0 ? announce.gpioIndex : 4;
          this.deviceManager.addOrUpdateDevice(mac, String(announce.deviceId), announce.deviceName, gpioCount);
          this.onLog('info', `AnnounceV2 from ${mac}: name=${announce.deviceName}, gpios=${gpioCount}`);
        }
        break;
      }
      case IOT_CMD_SENSOR_REPORT: {
        const sensorType = packet.gpioIndex;
        const value16 = packet.value | ((message[4] || 0) << 8);
        this.deviceManager.updateSensorData(mac, String(packet.deviceId), sensorType, value16);
        break;
      }
      default: {
        this.onLog('info', `Unknown command ${packet.command} from ${mac}`);
      }
    }
  }

  async sendCommand(deviceId: string, command: number, gpioIndex: number, value: number) {
    if (!this.client) return false;
    const buf = Buffer.alloc(8);
    buf.writeUInt8(command, 0);
    buf.writeUInt8(parseInt(deviceId, 10) || 0, 1);
    buf.writeUInt8(gpioIndex, 2);
    buf.writeUInt8(value, 3);
    buf.writeUInt32LE(0, 4);

    const topic = deviceId === 'broadcast'
      ? MQTT_TOPIC_CMD_BROADCAST
      : `${MQTT_TOPIC_CMD_PREFIX}${deviceId}`;

    return new Promise<boolean>((resolve) => {
      this.client!.publish(topic, buf, { qos: 1 }, (err) => {
        if (err) {
          this.onLog('error', `Publish failed: ${err.message}`);
          resolve(false);
        } else {
          this.onLog('info', `Sent cmd=${command.toString(16)} to ${topic}, gpio=${gpioIndex}, value=${value}`);
          resolve(true);
        }
      });
    });
  }

  async sendDiscover() {
    if (!this.client) return;
    const buf = Buffer.alloc(8);
    buf.writeUInt8(0x06, 0);
    buf.writeUInt32LE(0, 4);
    this.client.publish(MQTT_TOPIC_CMD_BROADCAST, buf);
    this.onLog('info', 'Sent discover command');
  }

  disconnect() {
    this.client?.end();
  }
}
