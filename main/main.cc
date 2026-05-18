#include <esp_log.h>
#include <esp_err.h>
#include <nvs.h>
#include <nvs_flash.h>
#include <driver/gpio.h>
#include <esp_event.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "sdkconfig.h"
#if CONFIG_SMART_HOME_PANEL_MODE
#include "smart_home_app.h"
#else
#include "application.h"
#endif

#define TAG "main"

extern "C" void app_main(void)
{
    // Initialize NVS flash for WiFi configuration
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "Erasing NVS flash to fix corruption");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

#if CONFIG_SMART_HOME_PANEL_MODE
    ESP_LOGI(TAG, "Starting ESP32-P4 smart home panel mode");
    smart_home_app_main();
#else
    // Initialize and run the application
    auto& app = Application::GetInstance();
    app.Initialize();
    app.Run();  // This function runs the main event loop and never returns
#endif
}
