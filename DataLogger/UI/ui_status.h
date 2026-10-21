/**
 * @file    ui_status.h
 * @brief   Mutex-protected OLED UI status snapshot.
 * @author  SLK800
 * @date    2026-05
 * @version 1.0
 */

#ifndef __UI_STATUS_H
#define __UI_STATUS_H

/*
 * 多个任务通过互斥锁写入 s_ui_status，Display 任务一次性取快照后
 * 在锁外完成字符串格式化与 I2C 刷新，尽量缩短临界区。
 */
#include "sensor_ntc.h"
#include "ui_types.h"

void UI_Status_Init(void);
void UI_Status_SetOledOk(uint8_t ok);
void UI_Status_UpdateProducer(uint32_t sample_count, int32_t temp_x10, const char *msg);
void UI_Status_UpdateSensor(const SensorNtcData_t *data);
void UI_Status_UpdateSystem(void);
void UI_Status_UpdateFlash(uint32_t used, uint32_t total, uint8_t file_count);
void UI_Status_GetSnapshot(UiStatus_t *out);

#endif /* __UI_STATUS_H */
