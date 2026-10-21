/**
 * @file    domain_mkt.h
 * @brief   MKT（Mean Kinetic Temperature，平均动力学温度）
 * @author  SLK800
 * @date    2026-05
 * @version 1.0
 *
 * @note
 * MKT 是冷链/医药储运中用来衡量"温度波动对货品等效影响"的指标。
 * 它把一段时间内波动的温度换算成一个等效恒温值：
 *
 *     Tk = (ΔH/R) / -ln( (1/n) · Σ exp(-ΔH / (R · Ti_kelvin)) )
 *
 * 参数（ICH Q1A 常用值）：
 *   ΔH = 83.144 kJ/mol （活化能）
 *   R  = 8.314462618 kJ/(mol·K)
 *   ΔH/R ≈ 10000 K
 *
 * 实现上不保存全部样本，而是用"指数和"增量累加：
 *     sum += exp(-Ea_R / Tk)
 * 这样只需一个 double（或定点），内存 O(1)。
 *
 * 注意：Cortex-M3 没有 FPU，double 运算较慢。这里为了精度用 double，
 *      但每次采样只做一次 exp，60 秒采样时开销可忽略。
 */

#ifndef __DOMAIN_MKT_H
#define __DOMAIN_MKT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    double   sum;           /* Σ exp(-Ea_R / Tk) */
    uint32_t count;
} MktCtx_t;

/**
 * @brief 复位
 */
void Mkt_Init(MktCtx_t *ctx);

/**
 * @brief 加入一个温度样本（摄氏度 ×100）
 * @return true 表示已计入；false 表示温度非法被忽略
 */
bool Mkt_Add(MktCtx_t *ctx, int16_t t_x100);

/**
 * @brief 计算当前 MKT
 * @return 摄氏度 ×100；样本数为 0 时返回 0
 */
int16_t Mkt_Get(const MktCtx_t *ctx);

/**
 * @brief 样本数
 */
uint32_t Mkt_Count(const MktCtx_t *ctx);

#endif /* __DOMAIN_MKT_H */
