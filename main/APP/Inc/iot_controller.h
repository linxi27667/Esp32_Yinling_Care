#ifndef __IOT_CONTROLLER_H__
#define __IOT_CONTROLLER_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void App_IOT_System_Init(void);
bool App_IOT_Set_GPIO(const char* device_name, const char* gpio_name, uint8_t level);
int App_IOT_Get_GPIO(const char* device_name, const char* gpio_name);
int App_IOT_Get_Device_Count(void);
int App_IOT_Get_Local_GPIO_Count(void);
const char* App_IOT_Get_Device_Name(int index);
const char* App_IOT_Get_Local_GPIO_Name(int index);
bool App_IOT_Add_Remote_Device(const char* name, const uint8_t* mac, int gpio_count);
void App_IOT_Discover_Devices(void);
bool App_IOT_Set_Servo(const char* device_name, const char* gpio_name, uint8_t angle);
bool App_IOT_Open_Medicine_Box(const char* device_name, uint8_t box_index);
void App_IOT_Get_All_Status(char* buffer, int buffer_size);

#ifdef __cplusplus
}
#endif

#endif
