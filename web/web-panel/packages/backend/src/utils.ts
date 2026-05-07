export function macToString(mac: Buffer): string {
  return mac.toString('hex').toUpperCase();
}

export function parseMacFromTopic(topic: string, prefix: string): string | null {
  if (topic.startsWith(prefix)) {
    return topic.slice(prefix.length);
  }
  return null;
}

export function createCommandPacket(
  command: number,
  deviceId: number,
  gpioIndex: number,
  value: number
): Buffer {
  const buf = Buffer.alloc(8);
  buf.writeUInt8(command, 0);
  buf.writeUInt8(deviceId, 1);
  buf.writeUInt8(gpioIndex, 2);
  buf.writeUInt8(value, 3);
  buf.writeUInt32LE(0, 4);
  return buf;
}

export function parseCommandPacket(data: Buffer): { command: number; deviceId: number; gpioIndex: number; value: number } | null {
  if (data.length < 8) return null;
  return {
    command: data.readUInt8(0),
    deviceId: data.readUInt8(1),
    gpioIndex: data.readUInt8(2),
    value: data.readUInt8(3),
  };
}

export function parseAnnounceV2Packet(data: Buffer): { command: number; deviceId: number; gpioIndex: number; value: number; deviceName: string } | null {
  if (data.length < 28) return null;
  const nameBuf = data.slice(4, 28);
  const nullIndex = nameBuf.indexOf(0);
  const deviceName = nameBuf.toString('utf8', 0, nullIndex >= 0 ? nullIndex : nameBuf.length).trim();
  return {
    command: data.readUInt8(0),
    deviceId: data.readUInt8(1),
    gpioIndex: data.readUInt8(2),
    value: data.readUInt8(3),
    deviceName,
  };
}
