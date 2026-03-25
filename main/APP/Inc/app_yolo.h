#ifndef __APP_YOLO_H__
#define __APP_YOLO_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define YOLO_MAX_BOXES 10

typedef struct {
    int x, y, w, h;
    int class_id;
    float confidence;
} yolo_box_t;

typedef struct {
    int count;
    yolo_box_t boxes[YOLO_MAX_BOXES];
} yolo_result_t;

bool App_Yolo_Init(void);
bool App_Yolo_Trigger_Async(uint8_t *rgb565_data, int width, int height);
void App_Yolo_Draw_Latest_Boxes(uint8_t *rgb565_data, int width, int height);
bool App_Yolo_Detect(uint8_t *rgb565_data, int width, int height, yolo_result_t *result);

#ifdef __cplusplus
}
#endif

#endif
