I (472) wifi_init: accept mbox: 6
I (475) wifi_init: tcpip mbox: 32
I (478) wifi_init: udp mbox: 6
I (481) wifi_init: tcp mbox: 6
I (483) wifi_init: tcp tx win: 5760
I (487) wifi_init: tcp rx win: 5760
I (490) wifi_init: tcp mss: 1440
I (493) wifi_init: WiFi IRAM OP enabled
I (496) wifi_init: WiFi RX IRAM OP enabled
I (500) BSP_WIFI: 配置WiFi: SSID=Ll, 密码长度=8
W (506) wifi:Password length matches WPA2 standards, authmode threshold changes from OPEN to WPA2
I (514) phy_init: phy_version 711,97bcf0a2,Aug 25 2025,19:04:10
I (557) wifi:mode : sta (80:65:99:df:c0:e0)
I (558) wifi:enable tsf
I (559) BSP_WIFI: Wi-Fi 驱动初始化完成，正在连接 Ll...
I (560) BSP_WIFI: WiFi启动成功，开始连接...
I (628) wifi:new:<11,0>, old:<1,0>, ap:<255,255>, sta:<11,0>, prof:1, snd_ch_cfg:0x0
I (630) wifi:state: init -> auth (0xb0)
I (636) wifi:state: auth -> assoc (0x0)
I (641) wifi:state: assoc -> run (0x10)
I (665) wifi:connected with Ll, aid = 7, channel 11, BW20, bssid = 9a:5f:41:ea:cc:75
I (666) wifi:security: WPA2-PSK, phy: bgn, rssi: -36
I (670) wifi:pm start, type: 1

I (670) wifi:dp: 1, bi: 102400, li: 3, scale listen interval from 307200 us to 307200 us
I (678) wifi:set rx beacon pti, rx_bcn_pti: 0, bcn_timeout: 25000, mt_pti: 0, mt_time: 10000
I (728) wifi:AP's beacon interval = 102400 us, DTIM period = 3
I (1731) esp_netif_handlers: sta ip: 192.168.137.40, mask: 255.255.255.0, gw: 192.168.137.1
I (1732) BSP_WIFI: 成功获取到 IP 地址: 192.168.137.40
I (1734) BSP_WIFI: 成功连接到热点: Ll
I (1738) APP_WIFI: WiFi 连接成功! 请访问地址: http://192.168.137.40/stream
I (1746) wifi:Set ps type: 0, coexist: 0

I (1749) MAIN: Initializing ESP-NOW...
I (1752) ESP_NOW_RECV: Initializing ESP-NOW...
I (1757) wifi:(2.4G)set country: cc=CN schan=1 nchan=13 policy=0

I (1764) ESPNOW: espnow [version: 2.0] init
I (1767) ESP_NOW_RECV: WiFi connected, ESP-NOW will use channel 11
I (1772) ESP_NOW_RECV: ESP-NOW initialized
I (1776) MAIN: Initializing Device Control...
I (1780) IOT_CTRL: Device refresh task started
I (1785) IOT_CTRL: Light[0] -> GPIO2
I (1788) IOT_CTRL: Light[1] -> GPIO4
I (1791) IOT_CTRL: Light[2] -> GPIO5
I (1794) IOT_CTRL: Relay[0] -> GPIO6
I (1798) IOT_CTRL: Relay[1] -> GPIO7
I (1801) IOT_CTRL: Relay[2] -> GPIO15
I (1805) IOT_CTRL: Servo initialized on GPIO 18
I (1809) IOT_CTRL: Servo[0] -> GPIO18
I (1812) IOT_CTRL: Servo initialized on GPIO 21
I (1816) IOT_CTRL: Servo[1] -> GPIO21
I (1820) IOT_CTRL: Servo initialized on GPIO 40
I (1824) IOT_CTRL: Servo[2] -> GPIO40
I (1827) IOT_CTRL: Device control initialized: 3 lights, 3 relays, 3 servos
I (1834) MAIN: Initializing Heartbeat...
I (1838) ESP_NOW_HB: Heartbeat task started (interval: 30000 ms)
I (1843) MAIN: ========================================
I (1848) MAIN:    IoT Slave Ready!
I (1851) MAIN: ========================================
I (1856) MAIN: Waiting for commands...
I (28586) ESP_NOW_RECV: Command: cmd=6, gpio=0, value=0
I (28587) ESP_NOW_RECV: Master discovered: 80:B5:4E:DF:3D:88
I (28587) ESP_NOW_RECV: Announce V2 sent: 二楼设备 (channel 11)
I (78360) ESP_NOW_RECV: Command: cmd=16, gpio=7, value=90
