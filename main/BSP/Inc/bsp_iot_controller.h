#ifndef __BSP_IOT_CONTROLLER_H__
#define __BSP_IOT_CONTROLLER_H__

#include <stdint.h>
#include <stdbool.h>

#define IOT_MAX_LOCAL_GPIOS     8
#define IOT_MAX_REMOTE_DEVICES  4
#define IOT_MAX_REMOTE_GPIOS    8
#define IOT_DEVICE_NAME_LEN     32

typedef enum {
    IOT_DEVICE_LOCAL = 0,
    IOT_DEVICE_REMOTE = 1
} iot_device_type_t;

typedef enum {
    IOT_GPIO_MODE_OUTPUT = 0,
    IOT_GPIO_MODE_INPUT = 1
} iot_gpio_mode_t;

typedef struct {
    void* port;
    uint16_t pin;
    iot_gpio_mode_t mode;
    uint8_t default_level;
    char name[16];
} iot_gpio_config_t;

typedef struct {
    uint8_t device_id;
    char device_name[IOT_DEVICE_NAME_LEN];
    uint8_t mac_addr[6];
    uint8_t gpio_count;
    iot_gpio_config_t gpios[IOT_MAX_REMOTE_GPIOS];
    bool online;
    uint32_t last_heartbeat_ms;
} iot_remote_device_t;

typedef struct iot_controller_dev {
    iot_gpio_config_t local_gpios[IOT_MAX_LOCAL_GPIOS];
    int local_gpio_count;
    
    iot_remote_device_t remote_devices[IOT_MAX_REMOTE_DEVICES];
    int remote_device_count;
    
    uint8_t local_gpio_states[IOT_MAX_LOCAL_GPIOS];
    
    void (*Gpio_Init)(void* port, uint16_t pin, iot_gpio_mode_t mode, uint8_t default_level);
    void (*Gpio_Write)(void* port, uint16_t pin, uint8_t level);
    int  (*Gpio_Read)(void* port, uint16_t pin);
    
    bool (*EspNow_Init)(void);
    bool (*EspNow_Send)(const uint8_t* mac, const uint8_t* data, uint16_t len);
    void (*EspNow_Set_Recv_Callback)(void (*callback)(const uint8_t* mac, const uint8_t* data, int len));
    
    void (*Delay_Ms)(uint32_t ms);
    void (*Log_Info)(const char* tag, const char* fmt, ...);
    void (*Log_Error)(const char* tag, const char* fmt, ...);
} iot_controller_t;

typedef struct {
    uint8_t command;
    uint8_t device_id;
    uint8_t gpio_index;
    uint8_t value;
    uint8_t reserved[4];
} iot_command_packet_t;

typedef struct {
    uint8_t command;
    uint8_t device_id;
    uint8_t gpio_index;
    uint8_t value;
    char device_name[24];
} iot_announce_v2_packet_t;

typedef enum {
    IOT_CMD_SET_GPIO = 0x01,
    IOT_CMD_GET_GPIO = 0x02,
    IOT_CMD_GET_ALL_GPIO = 0x03,
    IOT_CMD_HEARTBEAT = 0x04,
    IOT_CMD_RESPONSE = 0x05,
    IOT_CMD_DISCOVER = 0x06,
    IOT_CMD_ANNOUNCE = 0x07,
    IOT_CMD_ANNOUNCE_V2 = 0x08,
    IOT_CMD_SET_SERVO = 0x10,
    IOT_CMD_SET_LIGHT = 0x11,
    IOT_CMD_SET_RELAY = 0x12,
    IOT_CMD_OPEN_MEDICINE_BOX = 0x20,
    IOT_CMD_BROADCAST_ALL_OFF = 0x30,
    IOT_CMD_BROADCAST_ALL_ON = 0x31
} iot_command_t;

void IOT_Controller_Init(iot_controller_t* dev);
int IOT_Add_Local_GPIO(iot_controller_t* dev, void* port, uint16_t pin, const char* name, iot_gpio_mode_t mode, uint8_t default_level);
int IOT_Add_Remote_Device(iot_controller_t* dev, const char* name, const uint8_t* mac, int gpio_count);
void IOT_Set_Local_GPIO(iot_controller_t* dev, int index, uint8_t level);
int IOT_Get_Local_GPIO(iot_controller_t* dev, int index);
bool IOT_Set_Remote_GPIO(iot_controller_t* dev, int device_index, int gpio_index, uint8_t level);
int IOT_Get_Remote_GPIO(iot_controller_t* dev, int device_index, int gpio_index);
int IOT_Find_GPIO_By_Name(iot_controller_t* dev, const char* name, int* device_index, int* gpio_index);
int IOT_Find_Device_By_Name(iot_controller_t* dev, const char* name);
void IOT_Process_Remote_Response(iot_controller_t* dev, const uint8_t* mac, const uint8_t* data, int len);
void IOT_Send_Heartbeat(iot_controller_t* dev);
void IOT_Discover_Devices(iot_controller_t* dev);
bool IOT_Set_Remote_Servo(iot_controller_t* dev, int device_index, int gpio_index, uint8_t angle);
bool IOT_Open_Medicine_Box(iot_controller_t* dev, int device_index, uint8_t box_index);
void IOT_Update_Heartbeat(iot_controller_t* dev, const uint8_t* mac);
void IOT_Check_Device_Timeout(iot_controller_t* dev, uint32_t timeout_ms);
bool IOT_Set_Light(iot_controller_t* dev, int device_index, uint8_t light_index, uint8_t on);
bool IOT_Set_Relay(iot_controller_t* dev, int device_index, uint8_t relay_index, uint8_t on);
bool IOT_Set_Servo(iot_controller_t* dev, int device_index, uint8_t servo_index, uint8_t angle);
bool IOT_Broadcast_All_Off(iot_controller_t* dev);
bool IOT_Broadcast_All_On(iot_controller_t* dev);

#endif
