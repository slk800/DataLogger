/**
 * @file    domain_alarm.c
 * @brief   报警状态机实现
 * @author  SLK800
 * @date    2026-05
 * @version 1.0
 *
 * @note
 * 状态机（HIGH 方向为例，LOW 对称）：
 *
 *   NORMAL --(t > high)-------------------------> PENDING(HIGH)
 *   PENDING(HIGH) --(持续 >= min_duration)-------> HIGH   [上报 STARTED]
 *   PENDING(HIGH) --(t <= high) 且未到时长-------> NORMAL
 *   HIGH --(t <= high - hyst)--------------------> PENDING(NORMAL)
 *   PENDING(NORMAL) --(持续 >= min_duration)-----> NORMAL [上报 CLEARED]
 *
 * 进入和解除共用同一个 pending 计时器，但 pending 与 state 是两个
 * 独立变量：state 是"已确认"的状态，pending 是"正在演化"的目标。
 * 混用二者会让状态机在解除时卡死（见 Alarm_Feed 中的说明）。
 */

#include "domain_alarm.h"
#include "domain_record.h"

#define ALARM_MIN_HYST_X100     10      /* 至少 0.1 °C，避免 0 回差抖动 */

static void Alarm_ResetEvent(AlarmEvent_t *out)
{
    if (out == NULL)
    {
        return;
    }
    out->kind = ALARM_KIND_NONE;
    out->start_utc = 0U;
    out->end_utc = 0U;
    out->min_x100 = 0;
    out->max_x100 = 0;
}

void Alarm_Init(AlarmCtx_t *ctx, const AlarmConfig_t *cfg)
{
    if (ctx == NULL)
    {
        return;
    }

    ctx->cfg = (cfg != NULL) ? *cfg : (AlarmConfig_t){0, 0, 0, 0};

    /* 配置兜底：顺序颠倒或回差为负会让状态机无法退出 */
    if (ctx->cfg.high_x100 < ctx->cfg.low_x100)
    {
        int16_t tmp = ctx->cfg.high_x100;
        ctx->cfg.high_x100 = ctx->cfg.low_x100;
        ctx->cfg.low_x100 = tmp;
    }
    if (ctx->cfg.hyst_x100 <= 0)
    {
        ctx->cfg.hyst_x100 = ALARM_MIN_HYST_X100;
    }
    /* 回差不能大于半个量程，否则可能永远无法回到正常区 */
    {
        int16_t half = (int16_t)((ctx->cfg.high_x100 - ctx->cfg.low_x100) / 2);
        if (half <= 0)
        {
            half = ALARM_MIN_HYST_X100;
        }
        if (ctx->cfg.hyst_x100 > half)
        {
            ctx->cfg.hyst_x100 = half;
        }
    }

    ctx->state = ALARM_STATE_NORMAL;
    ctx->pending = ALARM_STATE_NORMAL;     /* == state，表示无待定切换 */
    ctx->pending_since_ms = 0U;
    ctx->pending_since_utc = 0U;
    ctx->start_ms = 0U;
    ctx->start_utc = 0U;
    ctx->min_x100 = 0;
    ctx->max_x100 = 0;
}

/**
 * @brief 根据温度判断"原始"状态（未消抖）
 * @note  HIGH 的进入阈值是 high，退出阈值是 high - hyst；
 *        LOW  的进入阈值是 low，退出阈值是 low + hyst。
 *        为了用一个函数同时服务两个方向，这里用当前状态决定阈值。
 */
static AlarmState_e Alarm_RawState(const AlarmCtx_t *ctx, int16_t t)
{
    int16_t high_enter = ctx->cfg.high_x100;
    int16_t high_exit  = (int16_t)(ctx->cfg.high_x100 - ctx->cfg.hyst_x100);
    int16_t low_enter  = ctx->cfg.low_x100;
    int16_t low_exit   = (int16_t)(ctx->cfg.low_x100 + ctx->cfg.hyst_x100);

    if (ctx->state == ALARM_STATE_HIGH)
    {
        return (t > high_exit) ? ALARM_STATE_HIGH : ALARM_STATE_NORMAL;
    }
    if (ctx->state == ALARM_STATE_LOW)
    {
        return (t < low_exit) ? ALARM_STATE_LOW : ALARM_STATE_NORMAL;
    }

    /* NORMAL：分别判断是否越限 */
    if (t > high_enter)
    {
        return ALARM_STATE_HIGH;
    }
    if (t < low_enter)
    {
        return ALARM_STATE_LOW;
    }
    return ALARM_STATE_NORMAL;
}

AlarmEventType_e Alarm_Feed(AlarmCtx_t *ctx, int16_t t_x100,
                            uint32_t now_ms, uint32_t now_utc,
                            AlarmEvent_t *out)
{
    AlarmState_e raw;
    uint32_t elapsed;

    if (ctx == NULL)
    {
        return ALARM_EVENT_NONE;
    }

    Alarm_ResetEvent(out);

    /* 非法温度不参与报警判断，也不重置状态机 */
    if (!Record_TempValid(t_x100))
    {
        return ALARM_EVENT_NONE;
    }

    raw = Alarm_RawState(ctx, t_x100);

    /*
     * 情况 A：原始状态就是当前稳定状态 —— 清掉候选，刷新统计。
     *
     * pending == state 表示"没有待定切换"。
     * 这里不能把 pending 设成 ALARM_STATE_NORMAL：报警中恢复方向的目标
     * 恰好也是 NORMAL，会让后续 raw != pending 判定失效、沿用旧时间戳
     * （W2 第二版真实 bug）。
     */
    if (raw == ctx->state)
    {
        ctx->pending = ctx->state;
        ctx->pending_since_ms = now_ms;
        ctx->pending_since_utc = now_utc;
        if (ctx->state != ALARM_STATE_NORMAL)
        {
            if (t_x100 < ctx->min_x100) ctx->min_x100 = t_x100;
            if (t_x100 > ctx->max_x100) ctx->max_x100 = t_x100;
        }
        return ALARM_EVENT_NONE;
    }

    /*
     * 情况 B：原始状态是一个"新的目标状态"。
     *
     * 注意：这里不能用 ctx->candidate 去和 raw 比 —— 报警成立后
     * candidate 会被清成 NORMAL，而 raw 仍是恢复方向的目标，
     * 两者永远不等，状态机就卡死了（W2 首版真实 bug）。
     * 正确的做法是用一个独立的 pending 计时，见下。
     */
    if (raw != ctx->pending)
    {
        ctx->pending = raw;
        ctx->pending_since_ms = now_ms;
        ctx->pending_since_utc = now_utc;
        return ALARM_EVENT_NONE;
    }

    elapsed = now_ms - ctx->pending_since_ms;
    if (elapsed < (uint32_t)ctx->cfg.min_duration_s * 1000UL)
    {
        return ALARM_EVENT_NONE;
    }

    /* 情况 C1：NORMAL -> 报警 */
    if (ctx->state == ALARM_STATE_NORMAL)
    {
        ctx->state = raw;
        ctx->start_ms = ctx->pending_since_ms;
        ctx->start_utc = ctx->pending_since_utc;
        ctx->min_x100 = t_x100;
        ctx->max_x100 = t_x100;
        ctx->pending = ctx->state;
        ctx->pending_since_ms = now_ms;
        ctx->pending_since_utc = now_utc;

        if (out != NULL)
        {
            out->kind = (raw == ALARM_STATE_HIGH) ? ALARM_KIND_HIGH : ALARM_KIND_LOW;
            out->start_utc = ctx->start_utc;
            out->end_utc = ctx->start_utc;
            out->min_x100 = t_x100;
            out->max_x100 = t_x100;
        }
        return ALARM_EVENT_STARTED;
    }

    /* 情况 C2：报警 -> NORMAL */
    if (out != NULL)
    {
        out->kind = (ctx->state == ALARM_STATE_HIGH) ? ALARM_KIND_HIGH : ALARM_KIND_LOW;
        out->start_utc = ctx->start_utc;
        out->end_utc = ctx->pending_since_utc;
        out->min_x100 = ctx->min_x100;
        out->max_x100 = ctx->max_x100;
    }

    ctx->state = ALARM_STATE_NORMAL;
    ctx->pending = ALARM_STATE_NORMAL;
    ctx->pending_since_ms = now_ms;
    ctx->pending_since_utc = now_utc;
    return ALARM_EVENT_CLEARED;
}

bool Alarm_IsActive(const AlarmCtx_t *ctx)
{
    return (ctx != NULL) && (ctx->state != ALARM_STATE_NORMAL);
}

uint16_t Alarm_AuxFlags(const AlarmCtx_t *ctx)
{
    uint16_t aux = 0U;

    if (ctx == NULL)
    {
        return 0U;
    }
    if (ctx->state == ALARM_STATE_HIGH)
    {
        aux |= RECORD_AUX_ALARM_HIGH | RECORD_AUX_ALARM_ACTIVE;
    }
    else if (ctx->state == ALARM_STATE_LOW)
    {
        aux |= RECORD_AUX_ALARM_LOW | RECORD_AUX_ALARM_ACTIVE;
    }
    return aux;
}
