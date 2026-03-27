#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/oled_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "led/single_led.h"
#include "esp32_camera.h"
#include "mcp_server.h"
#include "yolo_mcp_tool.h"
#include "iot_mcp_tool.h"
#include "medicine_mcp_tool.h"
#include "alarm_mcp_tool.h"
#include "app_mpu6050.h"
#include "app_buzzer.h"
#include "app_fall_detect.h"
#include "wifi_manager.h"
#include "bsp_mpu6050.h"
#include "iot_controller.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_camera.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <algorithm>
#include <sstream>
#include <vector>

#define TAG "CustomEsp32s3Cam"

static SemaphoreHandle_t g_i2c_mutex = NULL;

class CustomEsp32s3Cam : public WifiBoard {
private:
    i2c_master_bus_handle_t display_i2c_bus_;
    Button boot_button_;
    Display* display_;
    Esp32Camera* camera_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    TaskHandle_t mpu6050_task_handle_ = nullptr;

    void InitializeDisplayI2c() {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = DISPLAY_SDA_PIN,
            .scl_io_num = DISPLAY_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));
        ESP_LOGI(TAG, "Display I2C bus initialized on SDA=%d, SCL=%d", DISPLAY_SDA_PIN, DISPLAY_SCL_PIN);
    }

    void InitializeSsd1306Display() {
        esp_lcd_panel_io_i2c_config_t io_config = {
            .dev_addr = 0x3C,
            .on_color_trans_done = nullptr,
            .user_ctx = nullptr,
            .control_phase_bytes = 1,
            .dc_bit_offset = 6,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .flags = {
                .dc_low_on_data = 0,
                .disable_control_phase = 0,
            },
            .scl_speed_hz = 400 * 1000,
        };

        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(display_i2c_bus_, &io_config, &panel_io_));

        ESP_LOGI(TAG, "Install SSD1306 driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = -1;
        panel_config.bits_per_pixel = 1;

        esp_lcd_panel_ssd1306_config_t ssd1306_config = {
            .height = static_cast<uint8_t>(DISPLAY_HEIGHT),
        };
        panel_config.vendor_config = &ssd1306_config;

        ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_));
        ESP_LOGI(TAG, "SSD1306 driver installed");

        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        if (esp_lcd_panel_init(panel_) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize display");
            display_ = new NoDisplay();
            return;
        }

        ESP_LOGI(TAG, "Turning display on");
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

        display_ = new OledDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
    }

    void InitializeCamera() {
        camera_config_t config = {};
        config.pin_d0 = CAMERA_PIN_D0;
        config.pin_d1 = CAMERA_PIN_D1;
        config.pin_d2 = CAMERA_PIN_D2;
        config.pin_d3 = CAMERA_PIN_D3;
        config.pin_d4 = CAMERA_PIN_D4;
        config.pin_d5 = CAMERA_PIN_D5;
        config.pin_d6 = CAMERA_PIN_D6;
        config.pin_d7 = CAMERA_PIN_D7;
        config.pin_xclk = CAMERA_PIN_XCLK;
        config.pin_pclk = CAMERA_PIN_PCLK;
        config.pin_vsync = CAMERA_PIN_VSYNC;
        config.pin_href = CAMERA_PIN_HREF;
        config.pin_sccb_sda = CAMERA_PIN_SIOD;
        config.pin_sccb_scl = CAMERA_PIN_SIOC;
        config.sccb_i2c_port = 1;
        config.pin_pwdn = CAMERA_PIN_PWDN;
        config.pin_reset = CAMERA_PIN_RESET;
        config.xclk_freq_hz = XCLK_FREQ_HZ;
        config.pixel_format = PIXFORMAT_RGB565;
        config.frame_size = FRAMESIZE_QVGA;
        config.jpeg_quality = 12;
        config.fb_count = 2;
        config.fb_location = CAMERA_FB_IN_PSRAM;
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
        camera_ = new Esp32Camera(config);
        camera_->SetHMirror(false);
        ESP_LOGI(TAG, "Camera initialized");
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }
            app.ToggleChatState();
        });
    }

    void InitializeMpu6050() {
#ifdef CONFIG_USE_SW_I2C_FOR_MPU6050
        App_MPU6050_Init();
        ESP_LOGI(TAG, "MPU6050 initialized via software I2C");
#else
        App_MPU6050_Init(display_i2c_bus_);
        ESP_LOGI(TAG, "MPU6050 initialized on shared I2C bus");
#endif
    }

    void InitializeBuzzer() {
        App_Buzzer_Init(BUZZER_GPIO_PIN, BUZZER_ACTIVE_LEVEL);
        ESP_LOGI(TAG, "Buzzer initialized on GPIO %d", BUZZER_GPIO_PIN);
    }

    static void Mpu6050Task(void* arg) {
        ESP_LOGI(TAG, "MPU6050 task started");
        vTaskDelay(pdMS_TO_TICKS(1000));
        while (1) {
            App_MPU6050_Read();
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }

    static void AlarmTask(void* arg) {
        ESP_LOGI(TAG, "Alarm task started");
        vTaskDelay(pdMS_TO_TICKS(2000));
        while (1) {
            Alarm_Mcp_Process();
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    void StartMpu6050Task() {
        xTaskCreatePinnedToCore(
            Mpu6050Task,
            "mpu6050_task",
            4096,
            NULL,
            2,
            &mpu6050_task_handle_,
            1
        );
    }

    void StartAlarmTask() {
        xTaskCreatePinnedToCore(
            AlarmTask,
            "alarm_task",
            4096,
            NULL,
            2,
            NULL,
            1
        );
        ESP_LOGI(TAG, "Alarm task started");
    }

public:
    CustomEsp32s3Cam() :
        boot_button_(BOOT_BUTTON_GPIO) {
        InitializeDisplayI2c();
        InitializeSsd1306Display();
        InitializeButtons();
        InitializeCamera();
        InitializeMpu6050();
        InitializeBuzzer();
        InitializeYolo();
        StartMpu6050Task();
        InitializeTools();
    }

    void StartNetwork() override {
        // First initialize WiFi (parent class)
        WifiBoard::StartNetwork();
        
        // Create a task to wait for WiFi connection and initialize ESP-NOW
        xTaskCreate([](void* arg) {
            auto* board = static_cast<CustomEsp32s3Cam*>(arg);
            
            // Wait for WiFi connection (check every 100ms)
            auto& wifi = WifiManager::GetInstance();
            while (!wifi.IsConnected()) {
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            
            // Wait a bit more for DHCP to complete
            vTaskDelay(pdMS_TO_TICKS(500));
            
            ESP_LOGI(TAG, "WiFi connected, initializing ESP-NOW...");
            board->InitializeIot();
            
            vTaskDelete(NULL);
        }, "iot_init", 4096, this, 2, NULL);
    }

    void InitializeYolo() {
        if (Yolo_Mcp_Init()) {
            ESP_LOGI(TAG, "YOLO face detection initialized");
        } else {
            ESP_LOGW(TAG, "YOLO face detection initialization failed");
        }
    }

    void InitializeIot() {
        if (Iot_Mcp_Init()) {
            ESP_LOGI(TAG, "IOT controller initialized");
        } else {
            ESP_LOGW(TAG, "IOT controller initialization failed");
        }
        
        if (Medicine_Mcp_Init()) {
            ESP_LOGI(TAG, "Medicine manager initialized");
        } else {
            ESP_LOGW(TAG, "Medicine manager initialization failed");
        }
        
        if (Alarm_Mcp_Init()) {
            ESP_LOGI(TAG, "Alarm manager initialized");
            
            Alarm_Mcp_Set_Callback([](alarm_info_t* alarm) {
                ESP_LOGI(TAG, "Alarm callback triggered: id=%d, message=%s", alarm->id, alarm->message);
                Application::GetInstance().Alert("闹钟提醒", alarm->message, "alert", "");
                for (int i = 0; i < 10; i++) {
                    App_Buzzer_Beep(200);
                    vTaskDelay(pdMS_TO_TICKS(300));
                }
            });
            
            StartAlarmTask();
        } else {
            ESP_LOGW(TAG, "Alarm manager initialization failed");
        }
    }

    void InitializeTools() {
        auto& mcp_server = McpServer::GetInstance();
        mcp_server.AddTool("self.camera.detect_face",
            "Detect human faces in the current camera view. Returns the number of faces found and their positions.\n"
            "Use this tool when the user asks about detecting or finding faces/people in front of the camera.",
            PropertyList(),
            [this](const PropertyList& properties) -> ReturnValue {
                if (camera_ == nullptr) {
                    throw std::runtime_error("Camera not available");
                }
                
                if (!camera_->Capture()) {
                    throw std::runtime_error("Failed to capture photo");
                }
                
                camera_fb_t* fb = esp_camera_fb_get();
                if (fb == nullptr) {
                    throw std::runtime_error("Failed to get frame buffer");
                }
                
                yolo_result_t result;
                bool success = Yolo_Mcp_Detect(fb->buf, fb->width, fb->height, &result);
                esp_camera_fb_return(fb);
                
                if (!success) {
                    throw std::runtime_error("Face detection failed");
                }
                
                cJSON* json = cJSON_CreateObject();
                cJSON_AddNumberToObject(json, "face_count", result.count);
                cJSON* boxes = cJSON_AddArrayToObject(json, "faces");
                
                for (int i = 0; i < result.count; i++) {
                    cJSON* box = cJSON_CreateObject();
                    cJSON_AddNumberToObject(box, "x", result.boxes[i].x);
                    cJSON_AddNumberToObject(box, "y", result.boxes[i].y);
                    cJSON_AddNumberToObject(box, "width", result.boxes[i].w);
                    cJSON_AddNumberToObject(box, "height", result.boxes[i].h);
                    cJSON_AddNumberToObject(box, "confidence", result.boxes[i].confidence);
                    cJSON_AddItemToArray(boxes, box);
                }
                
                return json;
            });

        mcp_server.AddTool("self.iot.set_gpio",
            "Control GPIO outputs on this device or remote ESP32 devices.\n"
            "Use this tool to turn on/off lights, relays, LEDs, or other GPIO-controlled devices.\n"
            "Args:\n"
            "  device: Device name ('local' for this device, or remote device name)\n"
            "  gpio: GPIO name (e.g., 'led1', 'relay1')\n"
            "  value: true to turn on, false to turn off\n"
            "Available local GPIOs: led1, led2, relay1, relay2",
            PropertyList({
                Property("device", kPropertyTypeString),
                Property("gpio", kPropertyTypeString),
                Property("value", kPropertyTypeBoolean)
            }),
            [](const PropertyList& properties) -> ReturnValue {
                auto device = properties["device"].value<std::string>();
                auto gpio = properties["gpio"].value<std::string>();
                auto value = properties["value"].value<bool>();
                
                bool success = Iot_Mcp_Set_GPIO(device.c_str(), gpio.c_str(), value);
                
                cJSON* json = cJSON_CreateObject();
                cJSON_AddBoolToObject(json, "success", success);
                cJSON_AddStringToObject(json, "device", device.c_str());
                cJSON_AddStringToObject(json, "gpio", gpio.c_str());
                cJSON_AddBoolToObject(json, "value", value);
                cJSON_AddStringToObject(json, "message", success ? 
                    (value ? "已打开" : "已关闭") : "操作失败");
                
                return json;
            });

        mcp_server.AddTool("self.iot.get_status",
            "Get the status of all IoT devices and GPIOs.\n"
            "Returns a list of local GPIOs and remote devices with their current states.",
            PropertyList(),
            [](const PropertyList& properties) -> ReturnValue {
                char buffer[1024];
                Iot_Mcp_Get_Status_JSON(buffer, sizeof(buffer));
                
                cJSON* json = cJSON_Parse(buffer);
                if (json == nullptr) {
                    json = cJSON_CreateObject();
                    cJSON_AddStringToObject(json, "raw", buffer);
                }
                
                return json;
            });

        mcp_server.AddTool("self.iot.discover",
            "Discover remote ESP32 IoT devices on the network.\n"
            "Sends a broadcast message to find available remote devices.",
            PropertyList(),
            [](const PropertyList& properties) -> ReturnValue {
                ESP_LOGI(TAG, "Starting device discovery via MCP tool...");
                
                Iot_Mcp_Discover();
                
                int new_devices = Iot_Mcp_Wait_For_Response(3000);
                
                cJSON* json = cJSON_CreateObject();
                
                int total_count = Iot_Mcp_Get_Remote_Device_Count();
                
                if (new_devices > 0) {
                    cJSON_AddStringToObject(json, "message", "发现新设备！");
                    cJSON_AddNumberToObject(json, "new_devices_found", new_devices);
                } else if (total_count > 0) {
                    cJSON_AddStringToObject(json, "message", "未发现新设备，但有已知设备在线");
                } else {
                    cJSON_AddStringToObject(json, "message", "未发现任何远程设备");
                }
                
                cJSON_AddNumberToObject(json, "remote_device_count", total_count);
                
                cJSON* devices = cJSON_AddArrayToObject(json, "known_devices");
                for (int i = 0; i < total_count; i++) {
                    const char* name = Iot_Mcp_Get_Remote_Device_Name(i);
                    if (name) {
                        cJSON_AddItemToArray(devices, cJSON_CreateString(name));
                    }
                }
                
                return json;
            });

        mcp_server.AddTool("self.mpu6050.get_data",
            "Get the current motion data from the MPU6050 sensor.\n"
            "Returns accelerometer data (g), gyroscope data (°/s), and temperature (°C).\n"
            "Use this tool when the user asks about motion, tilt, acceleration, or temperature.",
            PropertyList(),
            [](const PropertyList& properties) -> ReturnValue {
                cJSON* json = cJSON_CreateObject();
                
                cJSON* accel = cJSON_AddObjectToObject(json, "accelerometer");
                cJSON_AddNumberToObject(accel, "x", my_mpu.accel_x);
                cJSON_AddNumberToObject(accel, "y", my_mpu.accel_y);
                cJSON_AddNumberToObject(accel, "z", my_mpu.accel_z);
                
                cJSON* gyro = cJSON_AddObjectToObject(json, "gyroscope");
                cJSON_AddNumberToObject(gyro, "x", my_mpu.gyro_x);
                cJSON_AddNumberToObject(gyro, "y", my_mpu.gyro_y);
                cJSON_AddNumberToObject(gyro, "z", my_mpu.gyro_z);
                
                cJSON_AddNumberToObject(json, "temperature", my_mpu.temp);
                
                return json;
            });

        mcp_server.AddTool("self.buzzer.beep",
            "Control the buzzer to make sounds.\n"
            "Use this tool when the user wants to make a beep sound or alert.\n"
            "Args:\n"
            "  times: Number of beeps (1-10)\n"
            "  duration_ms: Duration of each beep in milliseconds (default 100)",
            PropertyList({
                Property("times", kPropertyTypeInteger),
                Property("duration_ms", kPropertyTypeInteger)
            }),
            [](const PropertyList& properties) -> ReturnValue {
                int times = properties["times"].value<int>();
                int duration_ms = properties["duration_ms"].value<int>();
                
                if (times < 1) times = 1;
                if (times > 10) times = 10;
                if (duration_ms < 10) duration_ms = 100;
                if (duration_ms > 2000) duration_ms = 2000;
                
                App_Buzzer_Beep_Times(times, duration_ms, duration_ms);
                
                cJSON* json = cJSON_CreateObject();
                cJSON_AddNumberToObject(json, "times", times);
                cJSON_AddNumberToObject(json, "duration_ms", duration_ms);
                cJSON_AddStringToObject(json, "message", "蜂鸣器已响铃");
                
                return json;
            });

        mcp_server.AddTool("self.iot.set_servo",
            "Control the servo motor on a remote ESP32 device to open medicine boxes sequentially.\n"
            "For rotary medicine boxes with one exit, boxes are opened one by one.\n"
            "Args:\n"
            "  device: Remote device name (e.g., '从机设备')\n"
            "  box_indices: Comma-separated indices (e.g., '1' or '1,2,3' or '1-7')",
            PropertyList({
                Property("device", kPropertyTypeString, "从机设备"),
                Property("box_indices", kPropertyTypeString, "Comma-separated box indices (e.g., '1' or '1,2,3' or '1-7')")
            }),
            [](const PropertyList& properties) -> ReturnValue {
                auto device = properties["device"].value<std::string>();
                auto box_indices_str = properties["box_indices"].value<std::string>();

                std::vector<int> box_list;
                std::stringstream ss(box_indices_str);
                std::string token;
                while (std::getline(ss, token, ',')) {
                    token.erase(std::remove_if(token.begin(), token.end(), ::isspace), token.end());
                    if (token.find('-') != std::string::npos) {
                        std::stringstream ts(token);
                        std::string start_str, end_str;
                        std::getline(ts, start_str, '-');
                        std::getline(ts, end_str, '-');
                        int start = std::stoi(start_str);
                        int end = std::stoi(end_str);
                        for (int i = start; i <= end; i++) {
                            box_list.push_back(i);
                        }
                    } else {
                        box_list.push_back(std::stoi(token));
                    }
                }

                cJSON* json = cJSON_CreateObject();
                cJSON_AddStringToObject(json, "device", device.c_str());
                cJSON* opened = cJSON_CreateArray();
                cJSON* failed = cJSON_CreateArray();

                for (size_t i = 0; i < box_list.size(); i++) {
                    int box_index = box_list[i];
                    bool success = App_IOT_Open_Medicine_Box(device.c_str(), (uint8_t)box_index);
                    if (success) {
                        cJSON_AddItemToArray(opened, cJSON_CreateNumber(box_index));
                    } else {
                        cJSON_AddItemToArray(failed, cJSON_CreateNumber(box_index));
                    }
                    if (i < box_list.size() - 1) {
                        vTaskDelay(pdMS_TO_TICKS(500));
                    }
                }
                cJSON_AddItemToObject(json, "opened", opened);
                cJSON_AddItemToObject(json, "failed", failed);
                cJSON_AddStringToObject(json, "message", cJSON_GetArraySize(failed) == 0 ?
                    "所有药盒已打开" : "部分药盒打开失败");

                return json;
            });

        mcp_server.AddTool("self.iot.set_light",
            "控制指定设备的灯光。\n"
            "Args:\n"
            "  device: 设备名称 (一楼设备/二楼设备/三楼设备)\n"
            "  index: 灯编号 (0-2)\n"
            "  value: true=开, false=关\n"
            "中文灯名: " CHINESE_LIGHT_NAMES(),
            PropertyList({
                Property("device", kPropertyTypeString),
                Property("index", kPropertyTypeInteger),
                Property("value", kPropertyTypeBoolean)
            }),
            [](const PropertyList& properties) -> ReturnValue {
                auto device = properties["device"].value<std::string>();
                int index = properties["index"].value<int>();
                bool value = properties["value"].value<bool>();

                bool success = App_IOT_Set_Light(device.c_str(), (uint8_t)index, value);

                cJSON* json = cJSON_CreateObject();
                cJSON_AddBoolToObject(json, "success", success);
                cJSON_AddStringToObject(json, "device", device.c_str());
                cJSON_AddNumberToObject(json, "light", index);
                cJSON_AddBoolToObject(json, "value", value);
                cJSON_AddStringToObject(json, "message", success ?
                    (value ? "灯已打开" : "灯已关闭") : "操作失败");

                return json;
            });

        mcp_server.AddTool("self.iot.set_relay",
            "控制指定设备的继电器。\n"
            "Args:\n"
            "  device: 设备名称 (一楼设备/二楼设备/三楼设备)\n"
            "  index: 继电器编号 (0-2)\n"
            "  value: true=开, false=关\n"
            "中文继电器名: " CHINESE_RELAY_NAMES(),
            PropertyList({
                Property("device", kPropertyTypeString),
                Property("index", kPropertyTypeInteger),
                Property("value", kPropertyTypeBoolean)
            }),
            [](const PropertyList& properties) -> ReturnValue {
                auto device = properties["device"].value<std::string>();
                int index = properties["index"].value<int>();
                bool value = properties["value"].value<bool>();

                bool success = App_IOT_Set_Relay(device.c_str(), (uint8_t)index, value);

                cJSON* json = cJSON_CreateObject();
                cJSON_AddBoolToObject(json, "success", success);
                cJSON_AddStringToObject(json, "device", device.c_str());
                cJSON_AddNumberToObject(json, "relay", index);
                cJSON_AddBoolToObject(json, "value", value);
                cJSON_AddStringToObject(json, "message", success ?
                    (value ? "继电器已打开" : "继电器已关闭") : "操作失败");

                return json;
            });

        mcp_server.AddTool("self.iot.set_servo_by_index",
            "控制指定设备的舵机角度。\n"
            "Args:\n"
            "  device: 设备名称 (一楼设备/二楼设备/三楼设备)\n"
            "  index: 舵机编号 (0-2)\n"
            "  angle: 角度 (0/45/90/135/180)\n"
            "中文舵机名: " CHINESE_SERVO_NAMES(),
            PropertyList({
                Property("device", kPropertyTypeString),
                Property("index", kPropertyTypeInteger),
                Property("angle", kPropertyTypeInteger)
            }),
            [](const PropertyList& properties) -> ReturnValue {
                auto device = properties["device"].value<std::string>();
                int index = properties["index"].value<int>();
                int angle = properties["angle"].value<int>();

                bool success = App_IOT_Set_Servo_ByIndex(device.c_str(), (uint8_t)index, (uint8_t)angle);

                cJSON* json = cJSON_CreateObject();
                cJSON_AddBoolToObject(json, "success", success);
                cJSON_AddStringToObject(json, "device", device.c_str());
                cJSON_AddNumberToObject(json, "servo", index);
                cJSON_AddNumberToObject(json, "angle", angle);
                cJSON_AddStringToObject(json, "message", success ? "舵机已设置" : "操作失败");

                return json;
            });

        mcp_server.AddTool("self.iot.all_off",
            "关闭所有设备（灯、继电器、舵机）。\n"
            "广播命令到所有设备，同时关闭所有负载。",
            PropertyList(),
            [](const PropertyList& properties) -> ReturnValue {
                bool success = App_IOT_Broadcast_All_Off();

                cJSON* json = cJSON_CreateObject();
                cJSON_AddBoolToObject(json, "success", success);
                cJSON_AddStringToObject(json, "message", success ? "所有设备已关闭" : "操作失败");

                return json;
            });

        mcp_server.AddTool("self.iot.all_on",
            "打开所有设备（灯、继电器）。\n"
            "广播命令到所有设备，同时打开所有负载。\n"
            "注意：舵机不会被设置，保持当前位置。",
            PropertyList(),
            [](const PropertyList& properties) -> ReturnValue {
                bool success = App_IOT_Broadcast_All_On();

                cJSON* json = cJSON_CreateObject();
                cJSON_AddBoolToObject(json, "success", success);
                cJSON_AddStringToObject(json, "message", success ? "所有设备已打开" : "操作失败");

                return json;
            });

        mcp_server.AddTool("self.iot.all_lights_off",
            "关闭所有灯。\n"
            "广播命令到所有设备，只关闭所有灯（不影响继电器和舵机）。\n"
            "用于用户说\"关灯\"、\"关闭所有灯\"时。",
            PropertyList(),
            [](const PropertyList& properties) -> ReturnValue {
                bool success = App_IOT_Broadcast_Lights_Off();

                cJSON* json = cJSON_CreateObject();
                cJSON_AddBoolToObject(json, "success", success);
                cJSON_AddStringToObject(json, "message", success ? "所有灯已关闭" : "操作失败");

                return json;
            });

        mcp_server.AddTool("self.iot.all_lights_on",
            "打开所有灯。\n"
            "广播命令到所有设备，只打开所有灯（不影响继电器和舵机）。\n"
            "用于用户说\"开灯\"、\"打开所有灯\"时。",
            PropertyList(),
            [](const PropertyList& properties) -> ReturnValue {
                bool success = App_IOT_Broadcast_Lights_On();

                cJSON* json = cJSON_CreateObject();
                cJSON_AddBoolToObject(json, "success", success);
                cJSON_AddStringToObject(json, "message", success ? "所有灯已打开" : "操作失败");

                return json;
            });

        mcp_server.AddTool("self.medicine.add",
            "添加一种药物到药物管理系统。\n"
            "当用户想要添加新的药物提醒或记录药物信息时使用此工具。\n"
            "参数：\n"
            "  name: 药物名称（必填）\n"
            "  note: 药物备注说明（可选，如用法用量注意事项）\n"
            "  daily_times: 每天需要服用的次数（默认3次）\n"
            "  dosage_per_time: 每次服用的剂量（默认1粒）\n"
            "  remaining_count: 剩余药量（默认0）\n"
            "  slot_id: 药盒槽位编号（默认0，用于出药控制）\n"
            "返回值：\n"
            "  操作结果，包含药物信息",
            PropertyList({
                Property("name", kPropertyTypeString),
                Property("note", kPropertyTypeString),
                Property("daily_times", kPropertyTypeInteger, 3),
                Property("dosage_per_time", kPropertyTypeInteger, 1),
                Property("remaining_count", kPropertyTypeInteger, 0),
                Property("slot_id", kPropertyTypeInteger, 0)
            }),
            [](const PropertyList& properties) -> ReturnValue {
                auto name = properties["name"].value<std::string>();
                auto note = properties["note"].value<std::string>();
                int daily_times = properties["daily_times"].value<int>();
                int dosage_per_time = properties["dosage_per_time"].value<int>();
                int remaining_count = properties["remaining_count"].value<int>();
                int slot_id = properties["slot_id"].value<int>();
                
                int result = Medicine_Mcp_Add(name.c_str(), note.c_str(), 
                                              daily_times, dosage_per_time, 
                                              remaining_count, slot_id);
                
                cJSON* json = cJSON_CreateObject();
                if (result >= 0) {
                    cJSON_AddBoolToObject(json, "success", true);
                    cJSON_AddStringToObject(json, "name", name.c_str());
                    cJSON_AddStringToObject(json, "message", "药物添加成功");
                } else {
                    cJSON_AddBoolToObject(json, "success", false);
                    cJSON_AddStringToObject(json, "error", result == -1 ? "药物名称无效" : "药物列表已满");
                }
                
                return json;
            });

        mcp_server.AddTool("self.medicine.remove",
            "从药物管理系统中移除一种药物。\n"
            "当用户想要删除某种药物的记录时使用此工具。\n"
            "参数：\n"
            "  name: 药物名称\n"
            "返回值：\n"
            "  操作结果",
            PropertyList({
                Property("name", kPropertyTypeString)
            }),
            [](const PropertyList& properties) -> ReturnValue {
                auto name = properties["name"].value<std::string>();
                
                bool success = Medicine_Mcp_Remove(name.c_str());
                
                cJSON* json = cJSON_CreateObject();
                cJSON_AddBoolToObject(json, "success", success);
                cJSON_AddStringToObject(json, "name", name.c_str());
                cJSON_AddStringToObject(json, "message", success ? "药物已移除" : "药物不存在");
                
                return json;
            });

        mcp_server.AddTool("self.medicine.get_status",
            "获取所有药物的当前状态。\n"
            "当用户想查看药物列表、剩余药量或今日服药情况时使用此工具。\n"
            "返回值：\n"
            "  所有药物的详细信息，包括名称、备注、每日次数、每次剂量、剩余药量、今日已服次数等",
            PropertyList(),
            [](const PropertyList& properties) -> ReturnValue {
                char buffer[2048];
                Medicine_Mcp_Get_Status_Json(buffer, sizeof(buffer));
                
                cJSON* json = cJSON_Parse(buffer);
                if (json == nullptr) {
                    json = cJSON_CreateObject();
                    cJSON_AddStringToObject(json, "raw", buffer);
                }
                
                return json;
            });

        mcp_server.AddTool("self.medicine.take",
            "记录服药并请求从机出药。\n"
            "当用户说'我该吃药了'、'我要吃药'或类似意图时使用此工具。\n"
            "此工具会：1. 查找当前应该服用的药物 2. 向从机发送出药指令 3. 记录服药信息\n"
            "参数：\n"
            "  medicine_name: 药物名称（可选，不填则自动选择当前应服用的药物）\n"
            "  device_name: 从机设备名称（可选，默认使用第一个在线从机）\n"
            "返回值：\n"
            "  出药结果和药物信息",
            PropertyList({
                Property("medicine_name", kPropertyTypeString),
                Property("device_name", kPropertyTypeString)
            }),
            [](const PropertyList& properties) -> ReturnValue {
                std::string medicine_name;
                std::string device_name;
                
                if (properties["medicine_name"].has_default_value() == false) {
                    medicine_name = properties["medicine_name"].value<std::string>();
                }
                if (properties["device_name"].has_default_value() == false) {
                    device_name = properties["device_name"].value<std::string>();
                }
                
                medicine_info_t* med = nullptr;
                
                if (!medicine_name.empty()) {
                    med = Medicine_Mcp_Get_Info(medicine_name.c_str());
                } else {
                    medicine_info_t medicines[MAX_MEDICINE_COUNT];
                    int count = Medicine_Mcp_Get_All_Medicines(medicines, MAX_MEDICINE_COUNT);
                    
                    for (int i = 0; i < count; i++) {
                        int today_count = Medicine_Mcp_Get_Today_Count(medicines[i].name);
                        if (today_count < medicines[i].daily_times) {
                            medicine_manager_t* manager = Medicine_Mcp_Get_Manager();
                            med = &manager->medicines[i];
                            medicine_name = med->name;
                            break;
                        }
                    }
                }
                
                if (med == nullptr) {
                    cJSON* json = cJSON_CreateObject();
                    cJSON_AddBoolToObject(json, "success", false);
                    cJSON_AddStringToObject(json, "message", "没有找到需要服用的药物，或者今日药物已全部服用完毕");
                    return json;
                }
                
                if (med->remaining_count < med->dosage_per_time) {
                    cJSON* json = cJSON_CreateObject();
                    cJSON_AddBoolToObject(json, "success", false);
                    cJSON_AddStringToObject(json, "medicine_name", med->name);
                    cJSON_AddStringToObject(json, "message", "药物剩余不足，请及时补充");
                    cJSON_AddNumberToObject(json, "remaining", med->remaining_count);
                    return json;
                }
                
                const char* target_device = device_name.empty() ? nullptr : device_name.c_str();
                
                if (target_device == nullptr) {
                    int device_count = Iot_Mcp_Get_Remote_Device_Count();
                    if (device_count > 0) {
                        target_device = Iot_Mcp_Get_Remote_Device_Name(0);
                    }
                }
                
                if (target_device == nullptr) {
                    Medicine_Mcp_Record_Take(med->name, med->dosage_per_time);
                    cJSON* json = cJSON_CreateObject();
                    cJSON_AddBoolToObject(json, "success", true);
                    cJSON_AddStringToObject(json, "medicine_name", med->name);
                    cJSON_AddStringToObject(json, "note", med->note);
                    cJSON_AddNumberToObject(json, "dosage", med->dosage_per_time);
                    cJSON_AddNumberToObject(json, "remaining", med->remaining_count);
                    cJSON_AddStringToObject(json, "message", "已记录服药，但未找到从机设备，请手动取药");
                    return json;
                }
                
                bool success = Medicine_Mcp_Request_Dispense(target_device, med->name, med->dosage_per_time);
                
                cJSON* json = cJSON_CreateObject();
                cJSON_AddBoolToObject(json, "success", success);
                cJSON_AddStringToObject(json, "medicine_name", med->name);
                cJSON_AddStringToObject(json, "note", med->note);
                cJSON_AddNumberToObject(json, "dosage", med->dosage_per_time);
                cJSON_AddNumberToObject(json, "remaining", med->remaining_count);
                cJSON_AddStringToObject(json, "device", target_device);
                cJSON_AddNumberToObject(json, "slot_id", med->slot_id);
                
                if (success) {
                    cJSON_AddStringToObject(json, "message", "出药成功，请取药并按时服用");
                } else {
                    cJSON_AddStringToObject(json, "message", "出药指令发送失败，请手动取药");
                }
                
                return json;
            });

        mcp_server.AddTool("self.medicine.record",
            "手动记录服药（不触发出药）。\n"
            "当用户已经服药，需要记录服药信息时使用此工具。\n"
            "参数：\n"
            "  medicine_name: 药物名称\n"
            "  dosage: 服用剂量（可选，默认使用药物预设剂量）\n"
            "返回值：\n"
            "  记录结果",
            PropertyList({
                Property("medicine_name", kPropertyTypeString),
                Property("dosage", kPropertyTypeInteger)
            }),
            [](const PropertyList& properties) -> ReturnValue {
                auto name = properties["medicine_name"].value<std::string>();
                
                medicine_info_t* med = Medicine_Mcp_Get_Info(name.c_str());
                if (med == nullptr) {
                    cJSON* json = cJSON_CreateObject();
                    cJSON_AddBoolToObject(json, "success", false);
                    cJSON_AddStringToObject(json, "message", "药物不存在");
                    return json;
                }
                
                int dosage = med->dosage_per_time;
                if (properties["dosage"].has_default_value() == false) {
                    dosage = properties["dosage"].value<int>();
                }
                
                bool success = Medicine_Mcp_Record_Take(name.c_str(), dosage);
                
                cJSON* json = cJSON_CreateObject();
                cJSON_AddBoolToObject(json, "success", success);
                cJSON_AddStringToObject(json, "medicine_name", name.c_str());
                cJSON_AddNumberToObject(json, "dosage", dosage);
                cJSON_AddNumberToObject(json, "remaining", med->remaining_count);
                cJSON_AddStringToObject(json, "message", success ? "服药记录已保存" : "记录失败");
                
                return json;
            });

        mcp_server.AddTool("self.medicine.get_history",
            "查看服药历史记录。\n"
            "当用户想查看吃了几次药、服药历史时使用此工具。\n"
            "参数：\n"
            "  medicine_name: 药物名称（可选，不填则显示所有药物记录）\n"
            "  limit: 返回记录数量限制（可选，默认10条）\n"
            "返回值：\n"
            "  服药历史记录列表",
            PropertyList({
                Property("medicine_name", kPropertyTypeString),
                Property("limit", kPropertyTypeInteger, 10)
            }),
            [](const PropertyList& properties) -> ReturnValue {
                std::string medicine_name;
                if (properties["medicine_name"].has_default_value() == false) {
                    medicine_name = properties["medicine_name"].value<std::string>();
                }
                int limit = properties["limit"].value<int>();
                if (limit < 1) limit = 10;
                if (limit > 50) limit = 50;
                
                medicine_record_t records[50];
                const char* name_ptr = medicine_name.empty() ? nullptr : medicine_name.c_str();
                int count = Medicine_Mcp_Get_Records(records, limit, name_ptr);
                
                cJSON* json = cJSON_CreateObject();
                cJSON_AddNumberToObject(json, "total_count", count);
                
                if (!medicine_name.empty()) {
                    int today_count = Medicine_Mcp_Get_Today_Count(medicine_name.c_str());
                    int total_count = Medicine_Mcp_Get_Total_Count(medicine_name.c_str());
                    cJSON_AddStringToObject(json, "medicine_name", medicine_name.c_str());
                    cJSON_AddNumberToObject(json, "today_taken", today_count);
                    cJSON_AddNumberToObject(json, "total_taken", total_count);
                }
                
                cJSON* records_array = cJSON_AddArrayToObject(json, "records");
                for (int i = 0; i < count; i++) {
                    cJSON* record = cJSON_CreateObject();
                    cJSON_AddStringToObject(record, "medicine_name", records[i].medicine_name);
                    cJSON_AddNumberToObject(record, "timestamp", records[i].timestamp);
                    cJSON_AddNumberToObject(record, "dosage", records[i].dosage);
                    cJSON_AddBoolToObject(record, "taken", records[i].taken);
                    
                    time_t t = (time_t)records[i].timestamp;
                    struct tm* tm_info = localtime(&t);
                    char time_str[32];
                    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", tm_info);
                    cJSON_AddStringToObject(record, "time_str", time_str);
                    
                    cJSON_AddItemToArray(records_array, record);
                }
                
                return json;
            });

        mcp_server.AddTool("self.medicine.set_remaining",
            "更新药物剩余数量。\n"
            "当用户补充药物后需要更新剩余药量时使用此工具。\n"
            "参数：\n"
            "  name: 药物名称\n"
            "  count: 新的剩余数量\n"
            "返回值：\n"
            "  更新结果",
            PropertyList({
                Property("name", kPropertyTypeString),
                Property("count", kPropertyTypeInteger)
            }),
            [](const PropertyList& properties) -> ReturnValue {
                auto name = properties["name"].value<std::string>();
                int count = properties["count"].value<int>();
                
                bool success = Medicine_Mcp_Set_Remaining(name.c_str(), count);
                
                cJSON* json = cJSON_CreateObject();
                cJSON_AddBoolToObject(json, "success", success);
                cJSON_AddStringToObject(json, "name", name.c_str());
                cJSON_AddNumberToObject(json, "remaining", count);
                cJSON_AddStringToObject(json, "message", success ? "药物剩余数量已更新" : "药物不存在");
                
                return json;
            });

        mcp_server.AddTool("self.alarm.add_timer",
            "设置定时器提醒。\n"
            "当用户说'X分钟后提醒我'、'定时X分钟'时使用此工具。\n"
            "参数：\n"
            "  minutes: 定时分钟数\n"
            "  message: 提醒内容\n"
            "返回值：\n"
            "  定时器信息，包含触发时间",
            PropertyList({
                Property("minutes", kPropertyTypeInteger),
                Property("message", kPropertyTypeString)
            }),
            [](const PropertyList& properties) -> ReturnValue {
                int minutes = properties["minutes"].value<int>();
                auto message = properties["message"].value<std::string>();
                
                int alarm_id = Alarm_Mcp_Add_Timer(minutes, message.c_str());
                
                cJSON* json = cJSON_CreateObject();
                cJSON_AddBoolToObject(json, "success", alarm_id > 0);
                
                if (alarm_id > 0) {
                    cJSON_AddNumberToObject(json, "alarm_id", alarm_id);
                    cJSON_AddNumberToObject(json, "minutes", minutes);
                    cJSON_AddStringToObject(json, "message", message.c_str());
                } else {
                    cJSON_AddStringToObject(json, "error", "定时器创建失败");
                }
                
                return json;
            });

        mcp_server.AddTool("self.alarm.add_alarm",
            "设置闹钟提醒。\n"
            "当用户说'X点提醒我'、'设置X点的闹钟'时使用此工具。\n"
            "参数：\n"
            "  hour: 小时（0-23）\n"
            "  minute: 分钟（0-59）\n"
            "  message: 提醒内容\n"
            "  repeat_daily: 是否每天重复（可选，默认false）\n"
            "返回值：\n"
            "  闹钟信息",
            PropertyList({
                Property("hour", kPropertyTypeInteger),
                Property("minute", kPropertyTypeInteger),
                Property("message", kPropertyTypeString),
                Property("repeat_daily", kPropertyTypeBoolean)
            }),
            [](const PropertyList& properties) -> ReturnValue {
                int hour = properties["hour"].value<int>();
                int minute = properties["minute"].value<int>();
                auto message = properties["message"].value<std::string>();
                bool repeat_daily = false;
                if (properties["repeat_daily"].has_default_value() == false) {
                    repeat_daily = properties["repeat_daily"].value<bool>();
                }
                
                int alarm_id = Alarm_Mcp_Add_Alarm(hour, minute, message.c_str(), repeat_daily);
                
                cJSON* json = cJSON_CreateObject();
                cJSON_AddBoolToObject(json, "success", alarm_id > 0);
                
                if (alarm_id > 0) {
                    cJSON_AddNumberToObject(json, "alarm_id", alarm_id);
                    cJSON_AddNumberToObject(json, "hour", hour);
                    cJSON_AddNumberToObject(json, "minute", minute);
                    cJSON_AddStringToObject(json, "message", message.c_str());
                    cJSON_AddBoolToObject(json, "repeat_daily", repeat_daily);
                } else {
                    cJSON_AddStringToObject(json, "error", "闹钟创建失败");
                }
                
                return json;
            });

        mcp_server.AddTool("self.alarm.add_medicine",
            "设置吃药提醒闹钟。\n"
            "当用户说'提醒我每天X点吃药'、'设置吃药闹钟'时使用此工具。\n"
            "参数：\n"
            "  hour: 小时（0-23）\n"
            "  minute: 分钟（0-59）\n"
            "  medicine_name: 药物名称\n"
            "  repeat_daily: 是否每天重复（可选，默认true）\n"
            "返回值：\n"
            "  吃药提醒信息",
            PropertyList({
                Property("hour", kPropertyTypeInteger),
                Property("minute", kPropertyTypeInteger),
                Property("medicine_name", kPropertyTypeString),
                Property("repeat_daily", kPropertyTypeBoolean)
            }),
            [](const PropertyList& properties) -> ReturnValue {
                int hour = properties["hour"].value<int>();
                int minute = properties["minute"].value<int>();
                auto medicine_name = properties["medicine_name"].value<std::string>();
                bool repeat_daily = true;
                if (properties["repeat_daily"].has_default_value() == false) {
                    repeat_daily = properties["repeat_daily"].value<bool>();
                }
                
                int alarm_id = Alarm_Mcp_Add_Medicine_Alarm(hour, minute, medicine_name.c_str(), repeat_daily);
                
                cJSON* json = cJSON_CreateObject();
                cJSON_AddBoolToObject(json, "success", alarm_id > 0);
                
                if (alarm_id > 0) {
                    cJSON_AddNumberToObject(json, "alarm_id", alarm_id);
                    cJSON_AddNumberToObject(json, "hour", hour);
                    cJSON_AddNumberToObject(json, "minute", minute);
                    cJSON_AddStringToObject(json, "medicine_name", medicine_name.c_str());
                    cJSON_AddBoolToObject(json, "repeat_daily", repeat_daily);
                } else {
                    cJSON_AddStringToObject(json, "error", "吃药提醒创建失败");
                }
                
                return json;
            });

        mcp_server.AddTool("self.alarm.get_list",
            "获取所有闹钟和定时器列表。\n"
            "当用户说'查看我的闹钟'、'有哪些提醒'时使用此工具。\n"
            "返回值：\n"
            "  所有闹钟和定时器列表",
            PropertyList(),
            [](const PropertyList& properties) -> ReturnValue {
                char buffer[2048];
                Alarm_Mcp_Get_Status_Json(buffer, sizeof(buffer));
                
                cJSON* json = cJSON_Parse(buffer);
                if (json == nullptr) {
                    json = cJSON_CreateObject();
                    cJSON_AddStringToObject(json, "raw", buffer);
                }
                
                return json;
            });

        mcp_server.AddTool("self.alarm.dismiss",
            "关闭闹钟/定时器。\n"
            "当用户说'关闭闹钟'、'停止提醒'时使用此工具。\n"
            "参数：\n"
            "  alarm_id: 闹钟ID\n"
            "返回值：\n"
            "  操作结果",
            PropertyList({
                Property("alarm_id", kPropertyTypeInteger)
            }),
            [](const PropertyList& properties) -> ReturnValue {
                int alarm_id = properties["alarm_id"].value<int>();
                
                bool success = Alarm_Mcp_Dismiss(alarm_id);
                
                cJSON* json = cJSON_CreateObject();
                cJSON_AddBoolToObject(json, "success", success);
                cJSON_AddNumberToObject(json, "alarm_id", alarm_id);
                cJSON_AddStringToObject(json, "message", success ? "闹钟已关闭" : "闹钟不存在");
                
                return json;
            });

        mcp_server.AddTool("self.alarm.snooze",
            "贪睡/延迟闹钟。\n"
            "当用户说'再睡X分钟'、'延迟提醒'时使用此工具。\n"
            "参数：\n"
            "  alarm_id: 闹钟ID\n"
            "  minutes: 延迟分钟数（可选，默认5分钟）\n"
            "返回值：\n"
            "  操作结果",
            PropertyList({
                Property("alarm_id", kPropertyTypeInteger),
                Property("minutes", kPropertyTypeInteger)
            }),
            [](const PropertyList& properties) -> ReturnValue {
                int alarm_id = properties["alarm_id"].value<int>();
                int minutes = 5;
                if (properties["minutes"].has_default_value() == false) {
                    minutes = properties["minutes"].value<int>();
                }
                
                bool success = Alarm_Mcp_Snooze(alarm_id, minutes);
                
                cJSON* json = cJSON_CreateObject();
                cJSON_AddBoolToObject(json, "success", success);
                cJSON_AddNumberToObject(json, "alarm_id", alarm_id);
                cJSON_AddNumberToObject(json, "snooze_minutes", minutes);
                
                std::string msg = success ? 
                    ("闹钟已延迟" + std::to_string(minutes) + "分钟") : "操作失败";
                cJSON_AddStringToObject(json, "message", msg.c_str());
                
                return json;
            });

        mcp_server.AddTool("self.alarm.remove",
            "删除闹钟/定时器。\n"
            "当用户说'删除闹钟'、'取消提醒'时使用此工具。\n"
            "参数：\n"
            "  alarm_id: 闹钟ID\n"
            "返回值：\n"
            "  操作结果",
            PropertyList({
                Property("alarm_id", kPropertyTypeInteger)
            }),
            [](const PropertyList& properties) -> ReturnValue {
                int alarm_id = properties["alarm_id"].value<int>();
                
                bool success = Alarm_Mcp_Remove(alarm_id);
                
                cJSON* json = cJSON_CreateObject();
                cJSON_AddBoolToObject(json, "success", success);
                cJSON_AddNumberToObject(json, "alarm_id", alarm_id);
                cJSON_AddStringToObject(json, "message", success ? "闹钟已删除" : "闹钟不存在");
                
                return json;
            });
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, 
            AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
#else
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
#endif
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Camera* GetCamera() override {
        return camera_;
    }

    virtual std::string GetBoardType() override {
        return "custom-esp32s3-cam";
    }

    virtual std::string GetBoardJson() override {
        return "{\"type\":\"custom-esp32s3-cam\",\"camera\":true,\"display\":\"oled\",\"mpu6050\":true,\"buzzer\":true}";
    }

    virtual std::string GetDeviceStatusJson() override {
        return "{}";
    }
};

DECLARE_BOARD(CustomEsp32s3Cam)
