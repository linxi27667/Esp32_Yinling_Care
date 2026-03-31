#ifndef IOT_MCP_TOOL_H
#define IOT_MCP_TOOL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool Iot_Mcp_Init(void);
bool Iot_Mcp_Set_GPIO(const char* device_name, const char* gpio_name, bool value);
int Iot_Mcp_Get_GPIO(const char* device_name, const char* gpio_name);
void Iot_Mcp_Get_Status_JSON(char* buffer, int buffer_size);
int Iot_Mcp_Get_Local_GPIO_Count(void);
const char* Iot_Mcp_Get_Local_GPIO_Name(int index);
int Iot_Mcp_Get_Remote_Device_Count(void);
const char* Iot_Mcp_Get_Remote_Device_Name(int index);
bool Iot_Mcp_Add_Remote_Device(const char* name, const uint8_t* mac, int gpio_count);
void Iot_Mcp_Discover(void);
int Iot_Mcp_Wait_For_Response(int timeout_ms);
bool Iot_Mcp_Set_Servo(const char* device_name, uint8_t angle);
bool Iot_Mcp_Scenario_Homecoming(void);
bool Iot_Mcp_Scenario_GoodNight(void);
bool Iot_Mcp_Scenario_Emergency(void);
bool Iot_Mcp_Control_Servo_Angle(const char* device_name, uint8_t servo_index, uint8_t angle);
bool Iot_Mcp_Scenario_Rain_Collect_Clothes(void);
bool Iot_Mcp_Scenario_Ventilate(uint8_t window_angle, bool fan_on);
bool Iot_Mcp_Open_Medicine_Box(const char* device_name, uint8_t box_index);
bool Iot_Mcp_Close_Medicine_Box(const char* device_name);

#ifdef __cplusplus
}
#endif

#endif
