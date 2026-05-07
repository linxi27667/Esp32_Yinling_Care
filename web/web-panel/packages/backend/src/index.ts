import express from 'express';
import cors from 'cors';
import { createServer } from 'http';
import { DeviceManager } from './device-manager';
import { MqttClient } from './mqtt-client';
import { WsServer } from './websocket';
import { createApiRoutes } from './api-routes';

async function main() {
  const app = express();

  // 配置 CORS - 生产环境限制来源
  const corsOrigin = process.env.CORS_ORIGIN || '*';
  app.use(cors({ origin: corsOrigin }));
  app.use(express.json());

  // 健康检查端点
  app.get('/health', (_req, res) => {
    res.json({
      status: 'ok',
      timestamp: Date.now(),
      uptime: process.uptime(),
      env: process.env.NODE_ENV || 'development',
    });
  });

  const deviceManager = new DeviceManager();
  const mqttClient = new MqttClient(deviceManager, (level, message) => {
    console.log(`[${level.toUpperCase()}] ${message}`);
  });

  const mqttBroker = process.env.MQTT_BROKER || 'mqtt://127.0.0.1:1883';
  await mqttClient.connect(mqttBroker);

  app.use(createApiRoutes(deviceManager));

  const server = createServer(app);
  new WsServer(server, deviceManager, mqttClient);

  const PORT = process.env.PORT || 3001;
  server.listen(PORT, () => {
    console.log(`Server running on http://localhost:${PORT}`);
    console.log(`WebSocket endpoint: ws://localhost:${PORT}/ws`);
    console.log(`MQTT Broker: ${mqttBroker}`);
    console.log(`Environment: ${process.env.NODE_ENV || 'development'}`);
  });
}

main().catch(console.error);
