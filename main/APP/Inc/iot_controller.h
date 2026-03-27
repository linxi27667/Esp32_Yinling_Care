#ifndef __IOT_CONTROLLER_H__
#define __IOT_CONTROLLER_H__

#include <stdint.h>
#include <stdbool.h>

/* ================= 中文设备名称宏（用于 MCP 意图识别） ================= */
#define CHINESE_LIGHT_NAMES(n) \
    "一楼设备:浴室灯/客厅灯/卧室灯; " \
    "二楼设备:二楼浴室灯/二楼客厅灯/二楼卧室灯; " \
    "三楼设备:三楼浴室灯/三楼客厅灯/三楼卧室灯"

#define CHINESE_RELAY_NAMES(n) \
    "一楼设备:电视电源/空调电源/热水器电源; " \
    "二楼设备:二楼电视电源/二楼空调电源/二楼热水器电源; " \
    "三楼设备:三楼电视电源/三楼空调电源/三楼热水器电源"

#define CHINESE_SERVO_NAMES(n) \
    "一楼设备:窗户/窗帘/门锁; " \
    "二楼设备:二楼窗户/二楼窗帘/二楼门锁; " \
    "三楼设备:三楼窗户/三楼窗帘/三楼门锁"

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
bool App_IOT_Set_Light(const char* device_name, uint8_t light_index, bool on);
bool App_IOT_Set_Relay(const char* device_name, uint8_t relay_index, bool on);
bool App_IOT_Set_Servo_ByIndex(const char* device_name, uint8_t servo_index, uint8_t angle);
bool App_IOT_Broadcast_All_Off(void);
bool App_IOT_Broadcast_All_On(void);
bool App_IOT_Broadcast_Lights_Off(void);
bool App_IOT_Broadcast_Lights_On(void);
void App_IOT_Get_All_Status(char* buffer, int buffer_size);

#ifdef __cplusplus
}
#endif

#endif
