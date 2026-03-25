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

#ifdef __cplusplus
}
#endif

#endif
