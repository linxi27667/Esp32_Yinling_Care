import { ref, onMounted, onUnmounted } from 'vue';
import { useDeviceStore } from '@/stores/deviceStore';
import type { WsMessage } from '@xiaozhi/shared';

export function useWebSocket() {
  const store = useDeviceStore();
  const connected = ref(false);
  let ws: WebSocket | null = null;
  let reconnectTimer: ReturnType<typeof setTimeout> | null = null;

  function connect() {
    const wsUrl = import.meta.env.VITE_WS_URL || 'ws://localhost:3001/ws';
    ws = new WebSocket(wsUrl);

    ws.onopen = () => {
      connected.value = true;
      store.addLog('info', 'WebSocket 已连接');
      if (reconnectTimer) {
        clearTimeout(reconnectTimer);
        reconnectTimer = null;
      }
    };

    ws.onmessage = (event) => {
      try {
        const msg = JSON.parse(event.data) as WsMessage;
        handleMessage(msg);
      } catch {
        // ignore
      }
    };

    ws.onclose = () => {
      connected.value = false;
      store.addLog('warn', 'WebSocket 已断开，尝试重连...');
      reconnectTimer = setTimeout(connect, 3000);
    };

    ws.onerror = () => {
      store.addLog('error', 'WebSocket 错误');
    };
  }

  function handleMessage(msg: WsMessage) {
    switch (msg.type) {
      case 'state_update': {
        const payload = msg.payload as any;
        store.updateDevices(payload.devices || []);
        store.updateLocalGpios(payload.localGpios || []);
        store.updateSensorData(payload.sensorData || []);
        break;
      }
      case 'device_response': {
        const payload = msg.payload as any;
        store.clearPending(`${payload.deviceId}-${payload.gpioIndex}`);
        if (payload.success) {
          store.addLog('info', `设备 ${payload.deviceId} 操作成功`);
        } else {
          store.addLog('error', `设备 ${payload.deviceId} 操作失败`);
        }
        break;
      }
      case 'log': {
        const payload = msg.payload as any;
        store.addLog(payload.level, payload.message);
        break;
      }
    }
  }

  function sendCommand(payload: any) {
    if (ws && ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({ type: 'command', payload }));
    } else {
      store.addLog('error', 'WebSocket 未连接，无法发送命令');
    }
  }

  onMounted(() => connect());
  onUnmounted(() => {
    if (reconnectTimer) clearTimeout(reconnectTimer);
    ws?.close();
  });

  return { connected, sendCommand };
}
