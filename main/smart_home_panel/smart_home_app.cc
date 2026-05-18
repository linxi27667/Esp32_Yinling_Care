#include "smart_home_app.h"

#include "board.h"
#include "display.h"
#include "smart_home_mqtt.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *TAG = "SmartHomeApp";

extern "C" void smart_home_app_main(void)
{
    auto &board = Board::GetInstance();
    auto display = board.GetDisplay();
    if (display) {
        display->SetupUI();
    }

    board.SetNetworkEventCallback([](NetworkEvent event, const std::string &data) {
        switch (event) {
            case NetworkEvent::Connected:
                ESP_LOGI(TAG, "Network connected: %s", data.c_str());
                smart_home_mqtt_set_network_connected(true, data.c_str());
                smart_home_mqtt_start();
                break;
            case NetworkEvent::Disconnected:
                ESP_LOGW(TAG, "Network disconnected");
                smart_home_mqtt_set_network_connected(false, nullptr);
                break;
            case NetworkEvent::Connecting:
                ESP_LOGI(TAG, "Network connecting: %s", data.c_str());
                smart_home_mqtt_set_network_connected(false, data.c_str());
                break;
            default:
                break;
        }
    });

    board.StartNetwork();
    ESP_LOGI(TAG, "Smart home panel started");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
