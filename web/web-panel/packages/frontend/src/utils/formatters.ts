export function formatTime(timestamp: number): string {
  const d = new Date(timestamp);
  return d.toLocaleTimeString('zh-CN', {
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
  });
}

export function formatDateTime(timestamp: number): string {
  const d = new Date(timestamp);
  return d.toLocaleString('zh-CN');
}

export function getDeviceColor(deviceId: string): string {
  const colors: Record<string, string> = {
    '1': '#6750A4',
    '2': '#7D5260',
    '3': '#5060A4',
  };
  return colors[deviceId] || '#6750A4';
}
