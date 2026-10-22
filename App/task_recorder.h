/**
 * @file    task_recorder.h
 * @brief   记录任务 - 时钟、双通道采样、报警状态机、事件落盘
 * @author  SLK800
 * @date    2026-05
 * @version 1.0
 */

#ifndef __TASK_RECORDER_H
#define __TASK_RECORDER_H

#include "domain_alarm.h"

/**
 * @brief 记录任务入口
 */
void Task_Recorder(void *pvParam);

/**
 * @brief 获取当前 UTC（0 表示时钟未设置）
 * @note  供 OLED / 协议使用；读的是缓存值，不会阻塞 I2C。
 */
uint32_t Recorder_GetUtc(void);

/**
 * @brief 设置 UTC，并同步到 PCF8563
 * @return true 表示写入 RTC 成功
 */
bool Recorder_SetUtc(uint32_t utc);

/**
 * @brief 当前报警上下文（只读，供 OLED 显示）
 */
const AlarmCtx_t *Recorder_GetAlarm(void);

/**
 * @brief 当前 MKT（摄氏度 ×100），样本不足时返回 0
 */
int16_t Recorder_GetMkt(void);

/**
 * @brief 事件文件里的一条记录数（用于 STAT 显示）
 */
uint32_t Recorder_GetEventCount(void);

#endif /* __TASK_RECORDER_H */
