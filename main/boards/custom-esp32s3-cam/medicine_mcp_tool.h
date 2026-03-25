#ifndef MEDICINE_MCP_TOOL_H
#define MEDICINE_MCP_TOOL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MEDICINE_NAME_MAX_LEN   32
#define MEDICINE_NOTE_MAX_LEN   64
#define MAX_MEDICINE_COUNT      10
#define MAX_RECORD_COUNT        100

typedef struct {
    char name[MEDICINE_NAME_MAX_LEN];
    char note[MEDICINE_NOTE_MAX_LEN];
    uint8_t daily_times;
    uint8_t dosage_per_time;
    uint8_t remaining_count;
    uint8_t slot_id;
} medicine_info_t;

typedef struct {
    char medicine_name[MEDICINE_NAME_MAX_LEN];
    int64_t timestamp;
    uint8_t dosage;
    bool taken;
} medicine_record_t;

typedef struct {
    medicine_info_t medicines[MAX_MEDICINE_COUNT];
    int medicine_count;
    medicine_record_t records[MAX_RECORD_COUNT];
    int record_count;
} medicine_manager_t;

bool Medicine_Mcp_Init(void);

int Medicine_Mcp_Add(const char* name, const char* note, uint8_t daily_times, 
                     uint8_t dosage_per_time, uint8_t remaining_count, uint8_t slot_id);

bool Medicine_Mcp_Remove(const char* name);

medicine_info_t* Medicine_Mcp_Get_Info(const char* name);

int Medicine_Mcp_Get_All_Medicines(medicine_info_t* medicines, int max_count);

bool Medicine_Mcp_Record_Take(const char* name, uint8_t dosage);

int Medicine_Mcp_Get_Records(medicine_record_t* records, int max_count, const char* medicine_name);

int Medicine_Mcp_Get_Today_Count(const char* name);

int Medicine_Mcp_Get_Total_Count(const char* name);

void Medicine_Mcp_Get_Status_Json(char* buffer, int buffer_size);

bool Medicine_Mcp_Set_Remaining(const char* name, uint8_t count);

bool Medicine_Mcp_Request_Dispense(const char* device_name, const char* medicine_name, uint8_t dosage);

medicine_manager_t* Medicine_Mcp_Get_Manager(void);

#ifdef __cplusplus
}
#endif

#endif
