import { Router } from 'express';
import type { DeviceManager } from './device-manager';

export function createApiRoutes(deviceManager: DeviceManager): Router {
  const router = Router();

  router.get('/api/devices', (_req, res) => {
    res.json(deviceManager.getAllState());
  });

  router.get('/api/health', (_req, res) => {
    res.json({ status: 'ok', timestamp: Date.now() });
  });

  return router;
}
