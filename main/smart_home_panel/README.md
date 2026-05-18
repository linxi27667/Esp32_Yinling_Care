# ESP32-P4 Smart Home Panel

This module is an independent LVGL v9 smart home control panel for a 1024 x 600 ESP32-P4 display. It is enabled by `CONFIG_SMART_HOME_PANEL_MODE`, which defaults to `y` on `IDF_TARGET_ESP32P4`.

## MQTT

Broker:

```text
mqtt://8.134.167.240
```

Subscribed topics:

```text
smart-home/p4/state/#
smart-home/floor/+/availability
smart-home/floor/+/device/+
smart-home/floor/+/sensor/+
smart-home/env/state
smart-home/scene/state
xiaozhi/iot/heartbeat/+
xiaozhi/iot/sensor/+
xiaozhi/iot/announce/+
```

Device update payload example:

```json
{
  "device": "living_light",
  "online": true,
  "power": true,
  "brightness": 80,
  "status": "已开启",
  "updated": "09:41"
}
```

Sensor update payload example:

```json
{
  "device": "rain_2",
  "online": true,
  "value": 12,
  "adc": 128,
  "status": "正常",
  "updated": "09:41"
}
```

Published command topics:

```text
smart-home/floor/<floor>/device/<device>/set
smart-home/p4/network/wifi/connect
smart-home/p4/network/wifi/scan
```

Command payload examples:

```json
{"power":true,"brightness":80}
{"position":68,"mode":"manual"}
{"ssid":"Home_5G","password":"your-password"}
```

The UI does not generate simulated device or sensor values. Labels remain in the waiting state until retained or live MQTT messages arrive.
