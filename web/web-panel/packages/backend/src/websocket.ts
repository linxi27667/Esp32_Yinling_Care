import { WebSocketServer, WebSocket } from 'ws';
import type { Server } from 'http';
import type { DeviceManager } from './device-manager';
import type { MqttClient } from './mqtt-client';
import type { WsMessage, WsCommandPayload, LogPayload } from '@xiaozhi/shared';
import {
  IOT_CMD_SET_GPIO,
  IOT_CMD_SET_LIGHT,
  IOT_CMD_SET_RELAY,
  IOT_CMD_SET_SERVO,
  IOT_CMD_BROADCAST_ALL_OFF,
  IOT_CMD_BROADCAST_ALL_ON,
  IOT_CMD_BROADCAST_LIGHTS_OFF,
  IOT_CMD_BROADCAST_LIGHTS_ON,
  IOT_CMD_EMERGENCY,
} from '@xiaozhi/shared';

export class WsServer {
  private wss: WebSocketServer;
  private deviceManager: DeviceManager;
  private mqttClient: MqttClient;
  private clients: Set<WebSocket> = new Set();

  constructor(server: Server, deviceManager: DeviceManager, mqttClient: MqttClient) {
    this.deviceManager = deviceManager;
    this.mqttClient = mqttClient;
    this.wss = new WebSocketServer({ server, path: '/ws' });

    this.wss.on('connection', (ws) => {
      this.clients.add(ws);
      this.broadcastState();

      ws.on('message', (data) => {
        try {
          const msg = JSON.parse(data.toString()) as WsMessage;
          if (msg.type === 'command') {
            this.handleCommand(ws, msg.payload as WsCommandPayload);
          }
        } catch {
          // ignore invalid json
        }
      });

      ws.on('close', () => {
        this.clients.delete(ws);
      });
    });

    deviceManager.onChange(() => this.broadcastState());
  }

  private broadcastState() {
    const state = this.deviceManager.getAllState();
    const sensorData = this.deviceManager.getSensorData();
    const msg: WsMessage = {
      type: 'state_update',
      payload: { ...state, sensorData },
    };
    this.broadcast(msg);
  }

  broadcast(msg: WsMessage) {
    const data = JSON.stringify(msg);
    for (const client of this.clients) {
      if (client.readyState === WebSocket.OPEN) {
        client.send(data);
      }
    }
  }

  private sendLog(level: LogPayload['level'], message: string) {
    const msg: WsMessage = {
      type: 'log',
      payload: { level, message, timestamp: Date.now() },
    };
    this.broadcast(msg);
  }

  private async handleCommand(ws: WebSocket, payload: WsCommandPayload) {
    const { deviceId, command, gpioIndex, value, scenario } = payload;

    if (command === 'scenario' && scenario) {
      await this.handleScenario(scenario);
      return;
    }

    let cmdCode = IOT_CMD_SET_GPIO;
    if (command === 'set_light') cmdCode = IOT_CMD_SET_LIGHT;
    else if (command === 'set_relay') cmdCode = IOT_CMD_SET_RELAY;
    else if (command === 'set_servo') cmdCode = IOT_CMD_SET_SERVO;

    const success = await this.mqttClient.sendCommand(
      deviceId,
      cmdCode,
      gpioIndex ?? 0,
      value ?? 0
    );

    const response: WsMessage = {
      type: 'device_response',
      payload: {
        deviceId,
        mac: '',
        command: cmdCode,
        gpioIndex: gpioIndex ?? 0,
        value: value ?? 0,
        success,
      },
    };
    if (ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify(response));
    }
  }

  private async handleScenario(scenario: string) {
    const SERVO_GPIO_BASE = 6; // 从机协议：舵机从GPIO索引6开始
    const RELAY_GPIO_BASE = 3; // 从机协议：继电器从GPIO索引3开始
    switch (scenario) {
      case 'homecoming':
        await this.mqttClient.sendCommand('1', IOT_CMD_SET_SERVO, SERVO_GPIO_BASE + 0, 180); // 大门
        await this.mqttClient.sendCommand('broadcast', IOT_CMD_BROADCAST_LIGHTS_ON, 0, 0);
        this.sendLog('info', '场景：回家 — 开门 + 开灯');
        break;
      case 'leavehome':
        await this.mqttClient.sendCommand('1', IOT_CMD_SET_SERVO, SERVO_GPIO_BASE + 0, 0); // 大门
        this.sendLog('info', '场景：离家 — 关门');
        break;
      case 'goodnight':
        await this.mqttClient.sendCommand('broadcast', IOT_CMD_BROADCAST_LIGHTS_OFF, 0, 0);
        this.sendLog('info', '场景：晚安 — 关灯');
        break;
      case 'emergency':
        await this.mqttClient.sendCommand('broadcast', IOT_CMD_EMERGENCY, 0, 1);
        this.sendLog('warn', '场景：紧急模式触发');
        break;
      case 'rain':
        await this.mqttClient.sendCommand('3', IOT_CMD_SET_SERVO, SERVO_GPIO_BASE + 2, 0); // 三楼晾衣架
        this.sendLog('info', '场景：雨天收衣');
        break;
      case 'ventilate':
        await this.mqttClient.sendCommand('3', IOT_CMD_SET_SERVO, SERVO_GPIO_BASE + 0, 70); // 左天窗
        await this.mqttClient.sendCommand('3', IOT_CMD_SET_SERVO, SERVO_GPIO_BASE + 1, 70); // 右天窗
        await this.mqttClient.sendCommand('3', IOT_CMD_SET_RELAY, RELAY_GPIO_BASE + 0, 1);  // 风扇
        this.sendLog('info', '场景：通风 — 开窗 + 开风扇');
        break;
    }
  }
}
