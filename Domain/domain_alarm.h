/**
 * @file    domain_alarm.h
 * @brief   温度上下限报警 - 回差消抖与最短持续时间
 * @author  SLK800
 * @date    2026-05
 * @version 1.0
 *
 * @note
 * 为什么需要回差（hysteresis）和最短持续时间（min duration）：
 *   - 温度在阈值附近抖动时，简单比较会每秒产生一次"报警/恢复"，
 *     既刷屏又快速耗尽 Flash 的事件区。
 *   - 回差要求"越限后必须回到阈值内侧一段距离才算恢复"。
 *   - 最短持续时间要求"越限状态必须持续 N 秒才记为一次报警"。
 *
 * 时间基准：
 *   内部用毫秒（调用者传入，通常是 Tick 或 UTC 秒 ×1000）。
 *   事件输出同时带上调用者给的 UTC 秒，方便写入事件文件。
 */

#ifndef __DOMAIN_ALARM_H
#define __DOMAIN_ALARM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define ALARM_KIND_NONE     0U
#define ALARM_KIND_HIGH     1U
#define ALARM_KIND_LOW      2U

typedef struct {
    int16_t  low_x100;          /* 下限，温度 ×100 */
    int16_t  high_x100;         /* 上限 */
    int16_t  hyst_x100;         /* 回差（应 > 0） */
    uint16_t min_duration_s;    /* 最短持续时间 */
} AlarmConfig_t;

typedef enum {
    ALARM_STATE_NORMAL = 0,
    ALARM_STATE_HIGH,
    ALARM_STATE_LOW
} AlarmState_e;

typedef struct {
    AlarmConfig_t cfg;
    AlarmState_e  state;

    /* 正在朝某个目标状态演进，尚未满足 min_duration */
    AlarmState_e  pending;
    uint32_t      pending_since_ms;
    uint32_t      pending_since_utc;     /* 候选开始时刻的 UTC，用于事件起点 */

    /* 已成立的报警上下文 */
    uint32_t      start_ms;
    uint32_t      start_utc;
    int16_t       min_x100;
    int16_t       max_x100;
} AlarmCtx_t;

typedef enum {
    ALARM_EVENT_NONE = 0,
    ALARM_EVENT_STARTED,    /* 报警已持续够 min_duration，正式成立 */
    ALARM_EVENT_CLEARED     /* 报警解除，out 内带完整区间统计 */
} AlarmEventType_e;

typedef struct {
    uint8_t  kind;          /* ALARM_KIND_HIGH / LOW */
    uint32_t start_utc;
    uint32_t end_utc;
    int16_t  min_x100;
    int16_t  max_x100;
} AlarmEvent_t;

/**
 * @brief 初始化
 * @note  cfg 中 low < high，hyst >= 0；非法配置会被夹到合理值。
 */
void Alarm_Init(AlarmCtx_t *ctx, const AlarmConfig_t *cfg);

/**
 * @brief 喂入一个温度样本
 * @param ctx      上下文
 * @param t_x100   温度 ×100
 * @param now_ms   单调毫秒时间（用于持续时间判定）
 * @param now_utc  当前 UTC 秒（用于事件记录）
 * @param out      需要上报的事件，可为 NULL
 * @return 事件类型
 */
AlarmEventType_e Alarm_Feed(AlarmCtx_t *ctx, int16_t t_x100,
                            uint32_t now_ms, uint32_t now_utc,
                            AlarmEvent_t *out);

/**
 * @brief 当前是否处于报警状态
 */
bool Alarm_IsActive(const AlarmCtx_t *ctx);

/**
 * @brief 当前状态对应的 aux 标志位（供记录格式使用）
 */
uint16_t Alarm_AuxFlags(const AlarmCtx_t *ctx);

#endif /* __DOMAIN_ALARM_H */
