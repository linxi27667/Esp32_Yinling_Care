ESP-ROM:esp32s3-20210327
Build:Mar 27 2021
rst:0x1 (POWERON),boot:0x8 (SPI_FAST_FLASH_BOOT)
SPIWP:0xee
mode:DIO, clock div:1
load:0x3fce2820,len:0x14f0
load:0x403c8700,len:0xd24
load:0x403cb700,len:0x2ef0
entry 0x403c8924
I (24) boot: ESP-IDF v5.5.3 2nd stage bootloader
I (25) boot: compile time Mar 26 2026 20:34:48
I (25) boot: Multicore bootloader
I (25) boot: chip revision: v0.2
I (28) boot: efuse block revision: v1.3
I (31) boot.esp32s3: Boot SPI Speed : 80MHz
I (35) boot.esp32s3: SPI Mode       : DIO
I (39) boot.esp32s3: SPI Flash Size : 2MB
I (43) boot: Enabling RNG early entropy source...
I (47) boot: Partition Table:
I (50) boot: ## Label            Usage          Type ST Offset   Length
I (56) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (63) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (69) boot:  2 factory          factory app      00 00 00010000 00100000
I (76) boot: End of partition table
I (79) esp_image: segment 0: paddr=00010020 vaddr=3c090020 size=1a780h (108416) map
I (105) esp_image: segment 1: paddr=0002a7a8 vaddr=3fc9a600 size=04e54h ( 20052) load
I (110) esp_image: segment 2: paddr=0002f604 vaddr=40374000 size=00a14h (  2580) load
I (111) esp_image: segment 3: paddr=00030020 vaddr=42000020 size=85b68h (547688) map
I (213) esp_image: segment 4: paddr=000b5b90 vaddr=40374a14 size=15ae8h ( 88808) load
I (238) esp_image: segment 5: paddr=000cb680 vaddr=50000000 size=00020h (    32) load
I (247) boot: Loaded app from partition at offset 0x10000
I (247) boot: Disabling RNG early entropy source...
I (257) cpu_start: Multicore app
I (266) cpu_start: GPIO 44 and 43 are used as console UART I/O pins
I (267) cpu_start: Pro cpu start user code
I (267) cpu_start: cpu freq: 160000000 Hz
I (268) app_init: Application information:
I (272) app_init: Project name:     iot_slave_yaohe_Secondfloor
I (278) app_init: App version:      25e3af2-dirty
I (282) app_init: Compile time:     Mar 27 2026 11:47:11
I (287) app_init: ELF file SHA256:  df0c68087...
I (291) app_init: ESP-IDF:          v5.5.3
I (295) efuse_init: Min chip rev:     v0.0
I (299) efuse_init: Max chip rev:     v0.99
I (303) efuse_init: Chip rev:         v0.2
I (307) heap_init: Initializing. RAM available for dynamic allocation:
I (313) heap_init: At 3FCA3208 len 00046508 (281 KiB): RAM
I (318) heap_init: At 3FCE9710 len 00005724 (21 KiB): RAM
I (323) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM
I (329) heap_init: At 600FE000 len 00001FE8 (7 KiB): RTCRAM
I (335) spi_flash: detected chip: winbond
I (338) spi_flash: flash io: dio
W (341) spi_flash: Detected size(16384k) larger than the size in the binary image header(2048k). Using the size in the binary image header.
I (353) sleep_gpio: Configure to isolate all GPIO pins in sleep state
I (359) sleep_gpio: Enable automatic switching of GPIO sleep configuration
I (366) main_task: Started on CPU0
I (384) main_task: Calling app_main()
I (385) MAIN: ========================================
I (385) MAIN:    IoT Slave Device Starting...
I (386) MAIN: ========================================
I (409) MAIN: Connecting to WiFi...
I (409) APP_WIFI: 网络子系统启动，正在连接热点: Ll
I (411) pp: pp rom version: e7ae62f
I (412) net80211: net80211 rom version: e7ae62f
I (416) wifi:wifi driver task: 3fcedc04, prio:23, stack:6656, core=0
I (431) wifi:wifi firmware version: 4df78f2
I (431) wifi:wifi certification version: v7.0
I (432) wifi:config NVS flash: enabled
I (432) wifi:config nano formatting: disabled
I (436) wifi:Init data frame dynamic rx buffer num: 32
I (441) wifi:Init static rx mgmt buffer num: 5
I (445) wifi:Init management short buffer num: 32
I (449) wifi:Init dynamic tx buffer num: 32
I (453) wifi:Init static tx FG buffer num: 2
I (457) wifi:Init static rx buffer size: 1600
I (461) wifi:Init static rx buffer num: 10
I (465) wifi:Init dynamic rx buffer num: 32
I (470) wifi_init: rx ba win: 6
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
I (558) wifi:mode : sta (80:65:99:df:c0:e0)
I (558) wifi:enable tsf
I (560) BSP_WIFI: Wi-Fi 驱动初始化完成，正在连接 Ll...
I (560) BSP_WIFI: WiFi启动成功，开始连接...
I (580) wifi:new:<11,0>, old:<1,0>, ap:<255,255>, sta:<11,0>, prof:1, snd_ch_cfg:0x0
I (581) wifi:state: init -> auth (0xb0)
I (586) wifi:state: auth -> assoc (0x0)
I (592) wifi:state: assoc -> run (0x10)
I (620) wifi:connected with Ll, aid = 8, channel 11, BW20, bssid = 9a:5f:41:ea:cc:75
I (620) wifi:security: WPA2-PSK, phy: bgn, rssi: -34
I (624) wifi:pm start, type: 1

I (624) wifi:dp: 1, bi: 102400, li: 3, scale listen interval from 307200 us to 307200 us
I (632) wifi:set rx beacon pti, rx_bcn_pti: 0, bcn_timeout: 25000, mt_pti: 0, mt_time: 10000
I (678) wifi:AP's beacon interval = 102400 us, DTIM period = 3
I (1691) esp_netif_handlers: sta ip: 192.168.137.40, mask: 255.255.255.0, gw: 192.168.137.1
I (1692) BSP_WIFI: 成功获取到 IP 地址: 192.168.137.40
I (1694) BSP_WIFI: 成功连接到热点: Ll
I (1698) APP_WIFI: WiFi 连接成功! 请访问地址: http://192.168.137.40/stream
I (1706) wifi:Set ps type: 0, coexist: 0

I (1709) MAIN: Initializing ESP-NOW...
I (1712) ESP_NOW_RECV: Initializing ESP-NOW...
I (1717) wifi:(2.4G)set country: cc=CN schan=1 nchan=13 policy=0

I (1724) ESPNOW: espnow [version: 2.0] init
I (1727) ESP_NOW_RECV: WiFi connected, ESP-NOW will use channel 11
I (1732) ESP_NOW_RECV: ESP-NOW initialized
I (1736) MAIN: Initializing Device Control...
I (1740) IOT_CTRL: Device refresh task started
I (1745) IOT_CTRL: Light[0] -> GPIO2
I (1748) IOT_CTRL: Light[1] -> GPIO4
I (1751) IOT_CTRL: Light[2] -> GPIO5
I (1754) IOT_CTRL: Relay[0] -> GPIO6
I (1758) IOT_CTRL: Relay[1] -> GPIO7
I (1761) IOT_CTRL: Relay[2] -> GPIO15
I (1765) IOT_CTRL: Servo initialized on GPIO 18
I (1769) IOT_CTRL: Servo[0] -> GPIO18
I (1772) IOT_CTRL: Servo initialized on GPIO 21
I (1776) IOT_CTRL: Servo[1] -> GPIO21
I (1780) IOT_CTRL: Servo initialized on GPIO 40
I (1784) IOT_CTRL: Servo[2] -> GPIO40
I (1787) IOT_CTRL: Device control initialized: 3 lights, 3 relays, 3 servos
I (1794) MAIN: Initializing Heartbeat...
I (1798) ESP_NOW_HB: Heartbeat task started (interval: 30000 ms)
I (1803) MAIN: ========================================
I (1808) MAIN:    IoT Slave Ready!
I (1811) MAIN: ========================================
I (1816) MAIN: Waiting for commands...
I (136993) ESP_NOW_RECV: Command: cmd=6, gpio=0, value=0
I (136993) ESP_NOW_RECV: Master discovered: 80:B5:4E:DF:3D:88
I (136994) ESP_NOW_RECV: Announce V2 sent: 二楼设备 (channel 11)
I (148767) ESP_NOW_RECV: Command: cmd=16, gpio=7, value=90
I (173242) ESP_NOW_RECV: Command: cmd=16, gpio=7, value=0
I (178967) ESP_NOW_RECV: Command: cmd=16, gpio=7, value=180
I (201197) ESP_NOW_RECV: Command: cmd=16, gpio=7, value=0
I (207148) ESP_NOW_RECV: Command: cmd=16, gpio=7, value=180
I (212144) ESP_NOW_RECV: Command: cmd=16, gpio=7, value=0
I (216340) ESP_NOW_RECV: Command: cmd=16, gpio=7, value=180
I (370144) wifi:<ba-add>idx:0 (ifx:0, 9a:5f:41:ea:cc:75), tid:0, ssn:9, winSize:64
I (539552) ESP_NOW_RECV: Command: cmd=6, gpio=0, value=0
I (539553) ESP_NOW_RECV: Master discovered: 80:B5:4E:DF:3D:88
I (539553) ESP_NOW_RECV: Announce V2 sent: 二楼设备 (channel 11)
I (549897) ESP_NOW_RECV: Command: cmd=16, gpio=8, value=180
I (558791) ESP_NOW_RECV: Command: cmd=16, gpio=8, value=0
I (564611) ESP_NOW_RECV: Command: cmd=16, gpio=8, value=180
I (567377) ESP_NOW_RECV: Command: cmd=16, gpio=8, value=0
I (569527) ESP_NOW_RECV: Command: cmd=16, gpio=8, value=180
I (579665) ESP_NOW_RECV: Command: cmd=16, gpio=8, value=0
I (581815) ESP_NOW_RECV: Command: cmd=16, gpio=8, value=180
I (583556) ESP_NOW_RECV: Command: cmd=16, gpio=8, value=0
I (585911) ESP_NOW_RECV: Command: cmd=16, gpio=8, value=180