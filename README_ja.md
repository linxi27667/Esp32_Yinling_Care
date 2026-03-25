I (507) phy_init: phy_version 711,97bcf0a2,Aug 25 2025,19:04:10
I (557) wifi:mode : sta (3c:dc:75:6c:16:50)
I (558) wifi:enable tsf
I (560) BSP_WIFI: Wi-Fi 驱动初始化完成，正在连接 Ll...
I (560) BSP_WIFI: WiFi启动成功，开始连接...
I (569) wifi:new:<1,0>, old:<1,0>, ap:<255,255>, sta:<1,0>, prof:1, snd_ch_cfg:0x0
I (571) wifi:state: init -> auth (0xb0)
I (581) wifi:state: auth -> assoc (0x0)
I (595) wifi:state: assoc -> run (0x10)
I (652) wifi:connected with Ll, aid = 9, channel 1, BW20, bssid = 9a:5f:41:ea:cc:75
I (652) wifi:security: WPA2-PSK, phy: bgn, rssi: -20
I (656) wifi:pm start, type: 1

I (656) wifi:dp: 1, bi: 102400, li: 3, scale listen interval from 307200 us to 307200 us
I (664) wifi:set rx beacon pti, rx_bcn_pti: 0, bcn_timeout: 25000, mt_pti: 0, mt_time: 10000
I (673) wifi:AP's beacon interval = 102400 us, DTIM period = 3
I (1726) esp_netif_handlers: sta ip: 192.168.137.250, mask: 255.255.255.0, gw: 192.168.137.1
I (1726) BSP_WIFI: 成功获取到 IP 地址: 192.168.137.250
I (1729) BSP_WIFI: 成功连接到热点: Ll
I (1733) APP_WIFI: WiFi 连接成功! 请访问地址: http://192.168.137.250/stream       
I (1741) wifi:Set ps type: 0, coexist: 0

I (1745) MAIN: Initializing ESP-NOW...
I (1748) ESP_NOW_RECV: Device MAC: 3C:DC:75:6C:16:50
I (1753) ESPNOW: espnow [version: 2.0] init
I (1756) ESP_NOW_RECV: WiFi Channel: 1
I (1760) ESP_NOW_RECV: ESP-NOW initialized (channel 1)
I (1765) MAIN: Initializing Device Control...
I (1769) IOT_CTRL: Debug button initialized on GPIO 1 (pull-up)
I (1774) IOT_CTRL: Debug button task started on GPIO 1
I (1779) IOT_CTRL: Device refresh task started
I (1783) IOT_CTRL: GPIO[0] led1 -> GPIO2
I (1784) IOT_CTRL: Device control task started (refresh: 100 ms)
I (1787) IOT_CTRL: GPIO[1] led2 -> GPIO4
I (1797) IOT_CTRL: GPIO[2] led3 -> GPIO5
I (1800) IOT_CTRL: GPIO[3] led4 -> GPIO48
I (1804) IOT_CTRL: Servo initialized on GPIO 18
I (1808) MAIN: Initializing Heartbeat...
I (1812) ESP_NOW_HB: Heartbeat task started (interval: 30000 ms)
I (1817) MAIN: ========================================
I (1822) MAIN:    IoT Slave Ready!
I (1825) MAIN: ========================================
I (1830) MAIN: Waiting for commands...
I (70714) ESP_NOW_RECV: Command: cmd=6, gpio=0, value=0
I (70714) ESP_NOW_RECV: Master discovered: 80:B5:4E:DF:3D:88
I (70715) ESP_NOW_RECV: Announce sent to master
I (221553) ESP_NOW_RECV: Command: cmd=6, gpio=0, value=0
I (221553) ESP_NOW_RECV: Master discovered: 80:B5:4E:DF:3D:88
I (221554) ESP_NOW_RECV: Announce sent to master
I (244205) ESP_NOW_RECV: Command: cmd=6, gpio=0, value=0
I (244205) ESP_NOW_RECV: Master discovered: 80:B5:4E:DF:3D:88
I (244206) ESP_NOW_RECV: Announce sent to master
