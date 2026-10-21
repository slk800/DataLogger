/**
 * @file    domain_mkt.c
 * @brief   MKT 计算
 * @author  SLK800
 * @date    2026-05
 * @version 1.0
 *
 * @note
 * 为了在无 FPU 的 Cortex-M3 上避免链接完整的 double math 库还保留精度，
 * 这里只依赖 exp/log，ARMC 的 libm 会提供。单次调用约 1–2 µs @72 MHz。
 *
 * 数值稳定性：
 *   exp(-10000/Tk) 在 Tk≈298 K 时约为 e^-33.6 ≈ 2.4e-15，
 *   double 的最小规格化数约 2.2e-308，累加 1e6 次也不会溢出/下溢。
 *   但如果温度极高（例如 125°C = 398 K），单项会到 e^-25 ≈ 1.4e-11，
 *   仍在安全区间。
 */

#include "domain_mkt.h"
#include "domain_record.h"
#include <math.h>

#define MKT_EA_OVER_R       10000.0     /* ΔH/R，单位 K */
#define MKT_KELVIN_OFFSET   273.15

void Mkt_Init(MktCtx_t *ctx)
{
    if (ctx == NULL)
    {
        return;
    }
    ctx->sum = 0.0;
    ctx->count = 0U;
}

bool Mkt_Add(MktCtx_t *ctx, int16_t t_x100)
{
    double t_k;

    if (ctx == NULL || !Record_TempValid(t_x100))
    {
        return false;
    }

    t_k = ((double)t_x100 / 100.0) + MKT_KELVIN_OFFSET;
    if (t_k <= 0.0)
    {
        return false;
    }

    ctx->sum += exp(-MKT_EA_OVER_R / t_k);
    ctx->count++;
    return true;
}

int16_t Mkt_Get(const MktCtx_t *ctx)
{
    double mean;
    double t_k;
    double mkt_c;

    if (ctx == NULL || ctx->count == 0U)
    {
        return 0;
    }

    mean = ctx->sum / (double)ctx->count;
    if (mean <= 0.0)
    {
        return 0;
    }

    t_k = -MKT_EA_OVER_R / log(mean);
    mkt_c = t_k - MKT_KELVIN_OFFSET;

    return (int16_t)(mkt_c * 100.0);
}

uint32_t Mkt_Count(const MktCtx_t *ctx)
{
    return (ctx == NULL) ? 0U : ctx->count;
}
