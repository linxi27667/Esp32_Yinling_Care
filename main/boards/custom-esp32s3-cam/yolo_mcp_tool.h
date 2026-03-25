#ifndef YOLO_MCP_TOOL_H
#define YOLO_MCP_TOOL_H

#include "app_yolo.h"

#ifdef __cplusplus
extern "C" {
#endif

bool Yolo_Mcp_Init(void);
bool Yolo_Mcp_Detect(uint8_t *rgb565_data, int width, int height, yolo_result_t *result);
void Yolo_Mcp_Draw_Boxes(uint8_t *rgb565_data, int width, int height, const yolo_result_t *result);

#ifdef __cplusplus
}
#endif

#endif
