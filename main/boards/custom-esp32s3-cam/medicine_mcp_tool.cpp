#include "medicine_mcp_tool.h"
#include "iot_mcp_tool.h"
#include "iot_controller.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <time.h>
#include <stdio.h>

static const char* TAG = "MEDICINE_MCP";

static medicine_manager_t g_medicine_manager = {0};
static bool g_initialized = false;

static const char* NVS_NAMESPACE = "medicine";

static void Save_Medicines_To_NVS(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", NVS_NAMESPACE);
        return;
    }
    
    size_t required_size = sizeof(g_medicine_manager.medicines);
    nvs_set_blob(nvs_handle, "medicines", g_medicine_manager.medicines, required_size);
    nvs_set_i32(nvs_handle, "medicine_count", g_medicine_manager.medicine_count);
    
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Medicines saved to NVS");
}

static void Load_Medicines_From_NVS(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS namespace not found, using defaults");
        return;
    }
    
    size_t required_size = sizeof(g_medicine_manager.medicines);
    err = nvs_get_blob(nvs_handle, "medicines", g_medicine_manager.medicines, &required_size);
    if (err == ESP_OK) {
        int32_t count = 0;
        nvs_get_i32(nvs_handle, "medicine_count", &count);
        g_medicine_manager.medicine_count = count;
        ESP_LOGI(TAG, "Loaded %d medicines from NVS", g_medicine_manager.medicine_count);
    }
    
    nvs_close(nvs_handle);
}

static void Save_Records_To_NVS(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", NVS_NAMESPACE);
        return;
    }
    
    size_t required_size = sizeof(g_medicine_manager.records);
    nvs_set_blob(nvs_handle, "records", g_medicine_manager.records, required_size);
    nvs_set_i32(nvs_handle, "record_count", g_medicine_manager.record_count);
    
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Records saved to NVS");
}

static void Load_Records_From_NVS(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS namespace not found for records, using defaults");
        return;
    }
    
    size_t required_size = sizeof(g_medicine_manager.records);
    err = nvs_get_blob(nvs_handle, "records", g_medicine_manager.records, &required_size);
    if (err == ESP_OK) {
        int32_t count = 0;
        nvs_get_i32(nvs_handle, "record_count", &count);
        g_medicine_manager.record_count = count;
        ESP_LOGI(TAG, "Loaded %d records from NVS", g_medicine_manager.record_count);
    }
    
    nvs_close(nvs_handle);
}

bool Medicine_Mcp_Init(void) {
    if (g_initialized) {
        ESP_LOGW(TAG, "Medicine manager already initialized");
        return true;
    }
    
    ESP_LOGI(TAG, "Initializing Medicine MCP tool...");
    
    memset(&g_medicine_manager, 0, sizeof(g_medicine_manager));
    
    Load_Medicines_From_NVS();
    Load_Records_From_NVS();
    
    g_initialized = true;
    ESP_LOGI(TAG, "Medicine MCP tool initialized with %d medicines, %d records", 
             g_medicine_manager.medicine_count, g_medicine_manager.record_count);
    
    return true;
}

int Medicine_Mcp_Add(const char* name, const char* note, uint8_t daily_times, 
                     uint8_t dosage_per_time, uint8_t remaining_count, uint8_t slot_id) {
    if (name == NULL || strlen(name) == 0) {
        ESP_LOGE(TAG, "Invalid medicine name");
        return -1;
    }
    
    if (g_medicine_manager.medicine_count >= MAX_MEDICINE_COUNT) {
        ESP_LOGE(TAG, "Medicine list is full");
        return -2;
    }
    
    for (int i = 0; i < g_medicine_manager.medicine_count; i++) {
        if (strcmp(g_medicine_manager.medicines[i].name, name) == 0) {
            ESP_LOGW(TAG, "Medicine %s already exists, updating", name);
            strncpy(g_medicine_manager.medicines[i].note, note ? note : "", MEDICINE_NOTE_MAX_LEN - 1);
            g_medicine_manager.medicines[i].daily_times = daily_times;
            g_medicine_manager.medicines[i].dosage_per_time = dosage_per_time;
            g_medicine_manager.medicines[i].remaining_count = remaining_count;
            g_medicine_manager.medicines[i].slot_id = slot_id;
            Save_Medicines_To_NVS();
            return i;
        }
    }
    
    int index = g_medicine_manager.medicine_count;
    medicine_info_t* med = &g_medicine_manager.medicines[index];
    
    strncpy(med->name, name, MEDICINE_NAME_MAX_LEN - 1);
    med->name[MEDICINE_NAME_MAX_LEN - 1] = '\0';
    strncpy(med->note, note ? note : "", MEDICINE_NOTE_MAX_LEN - 1);
    med->note[MEDICINE_NOTE_MAX_LEN - 1] = '\0';
    med->daily_times = daily_times;
    med->dosage_per_time = dosage_per_time;
    med->remaining_count = remaining_count;
    med->slot_id = slot_id;
    
    g_medicine_manager.medicine_count++;
    
    Save_Medicines_To_NVS();
    
    ESP_LOGI(TAG, "Added medicine: %s, daily_times=%d, dosage=%d, remaining=%d, slot=%d",
             name, daily_times, dosage_per_time, remaining_count, slot_id);
    
    return index;
}

bool Medicine_Mcp_Remove(const char* name) {
    if (name == NULL) {
        return false;
    }
    
    for (int i = 0; i < g_medicine_manager.medicine_count; i++) {
        if (strcmp(g_medicine_manager.medicines[i].name, name) == 0) {
            for (int j = i; j < g_medicine_manager.medicine_count - 1; j++) {
                g_medicine_manager.medicines[j] = g_medicine_manager.medicines[j + 1];
            }
            g_medicine_manager.medicine_count--;
            Save_Medicines_To_NVS();
            ESP_LOGI(TAG, "Removed medicine: %s", name);
            return true;
        }
    }
    
    ESP_LOGW(TAG, "Medicine not found: %s", name);
    return false;
}

medicine_info_t* Medicine_Mcp_Get_Info(const char* name) {
    if (name == NULL) {
        return NULL;
    }
    
    for (int i = 0; i < g_medicine_manager.medicine_count; i++) {
        if (strcmp(g_medicine_manager.medicines[i].name, name) == 0) {
            return &g_medicine_manager.medicines[i];
        }
    }
    
    return NULL;
}

int Medicine_Mcp_Get_All_Medicines(medicine_info_t* medicines, int max_count) {
    if (medicines == NULL || max_count <= 0) {
        return 0;
    }
    
    int count = (g_medicine_manager.medicine_count < max_count) ? 
                g_medicine_manager.medicine_count : max_count;
    
    memcpy(medicines, g_medicine_manager.medicines, count * sizeof(medicine_info_t));
    return count;
}

bool Medicine_Mcp_Record_Take(const char* name, uint8_t dosage) {
    if (name == NULL || g_medicine_manager.record_count >= MAX_RECORD_COUNT) {
        ESP_LOGE(TAG, "Cannot record: invalid name or record list full");
        return false;
    }
    
    medicine_info_t* med = Medicine_Mcp_Get_Info(name);
    if (med == NULL) {
        ESP_LOGE(TAG, "Medicine not found: %s", name);
        return false;
    }
    
    time_t now;
    time(&now);
    
    medicine_record_t* record = &g_medicine_manager.records[g_medicine_manager.record_count];
    strncpy(record->medicine_name, name, MEDICINE_NAME_MAX_LEN - 1);
    record->medicine_name[MEDICINE_NAME_MAX_LEN - 1] = '\0';
    record->timestamp = (int64_t)now;
    record->dosage = dosage;
    record->taken = true;
    
    g_medicine_manager.record_count++;
    
    if (med->remaining_count >= dosage) {
        med->remaining_count -= dosage;
    } else {
        med->remaining_count = 0;
    }
    
    Save_Records_To_NVS();
    Save_Medicines_To_NVS();
    
    ESP_LOGI(TAG, "Recorded taking medicine: %s, dosage=%d, remaining=%d", 
             name, dosage, med->remaining_count);
    
    return true;
}

int Medicine_Mcp_Get_Records(medicine_record_t* records, int max_count, const char* medicine_name) {
    if (records == NULL || max_count <= 0) {
        return 0;
    }
    
    int count = 0;
    for (int i = g_medicine_manager.record_count - 1; i >= 0 && count < max_count; i--) {
        if (medicine_name == NULL || strlen(medicine_name) == 0 ||
            strcmp(g_medicine_manager.records[i].medicine_name, medicine_name) == 0) {
            records[count] = g_medicine_manager.records[i];
            count++;
        }
    }
    
    return count;
}

int Medicine_Mcp_Get_Today_Count(const char* name) {
    time_t now;
    time(&now);
    struct tm* today = localtime(&now);
    int today_start = today->tm_hour * 3600 + today->tm_min * 60 + today->tm_sec;
    time_t day_start = now - today_start;
    
    int count = 0;
    for (int i = 0; i < g_medicine_manager.record_count; i++) {
        if (strcmp(g_medicine_manager.records[i].medicine_name, name) == 0 &&
            g_medicine_manager.records[i].timestamp >= day_start) {
            count++;
        }
    }
    
    return count;
}

int Medicine_Mcp_Get_Total_Count(const char* name) {
    int count = 0;
    for (int i = 0; i < g_medicine_manager.record_count; i++) {
        if (strcmp(g_medicine_manager.records[i].medicine_name, name) == 0) {
            count++;
        }
    }
    
    return count;
}

void Medicine_Mcp_Get_Status_Json(char* buffer, int buffer_size) {
    if (buffer == NULL || buffer_size <= 0) return;
    
    int offset = 0;
    
    offset += snprintf(buffer + offset, buffer_size - offset, 
                       "{\"medicines\":[");
    
    for (int i = 0; i < g_medicine_manager.medicine_count && offset < buffer_size - 200; i++) {
        if (i > 0) offset += snprintf(buffer + offset, buffer_size - offset, ",");
        
        medicine_info_t* med = &g_medicine_manager.medicines[i];
        int today_count = Medicine_Mcp_Get_Today_Count(med->name);
        
        offset += snprintf(buffer + offset, buffer_size - offset,
                          "{\"name\":\"%s\",\"note\":\"%s\",\"daily_times\":%d,"
                          "\"dosage_per_time\":%d,\"remaining\":%d,\"slot_id\":%d,"
                          "\"today_taken\":%d}",
                          med->name, med->note, med->daily_times,
                          med->dosage_per_time, med->remaining_count, med->slot_id,
                          today_count);
    }
    
    offset += snprintf(buffer + offset, buffer_size - offset, 
                       "],\"total_records\":%d}", g_medicine_manager.record_count);
}

bool Medicine_Mcp_Set_Remaining(const char* name, uint8_t count) {
    medicine_info_t* med = Medicine_Mcp_Get_Info(name);
    if (med == NULL) {
        return false;
    }
    
    med->remaining_count = count;
    Save_Medicines_To_NVS();
    ESP_LOGI(TAG, "Set remaining count for %s to %d", name, count);
    return true;
}

bool Medicine_Mcp_Request_Dispense(const char* device_name, const char* medicine_name, uint8_t dosage) {
    medicine_info_t* med = Medicine_Mcp_Get_Info(medicine_name);
    if (med == NULL) {
        ESP_LOGE(TAG, "Medicine not found: %s", medicine_name);
        return false;
    }

    if (med->remaining_count < dosage) {
        ESP_LOGW(TAG, "Not enough medicine remaining: %s (need %d, have %d)",
                 medicine_name, dosage, med->remaining_count);
        return false;
    }

    ESP_LOGI(TAG, "Requesting dispense: device=%s, medicine=%s, slot=%d, dosage=%d",
             device_name ? device_name : "从机", medicine_name, med->slot_id, dosage);

    bool success = App_IOT_Open_Medicine_Box(device_name, med->slot_id);

    if (success) {
        Medicine_Mcp_Record_Take(medicine_name, dosage);
        ESP_LOGI(TAG, "Dispense command sent successfully");
    }

    return success;
}

medicine_manager_t* Medicine_Mcp_Get_Manager(void) {
    return &g_medicine_manager;
}
