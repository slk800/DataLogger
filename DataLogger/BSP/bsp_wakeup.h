/**
 * @file    bsp_wakeup.h
 * @brief   PCF8563 CLKOUT 唤醒源（EXTI1 / PB1）
 * @author  SLK800
 * @date    2026-05
 * @version 1.0
 *
 * @note
 * 接线：PCF8563 CLKOUT → PB1（EXTI1）
 *
 * 为什么用 CLKOUT 而不是 RTC alarm：
 *   - PCF8563 的 alarm 只有分/时/日/星期，**没有秒**；
 *     1 秒采样档无法用 alarm 唤醒。
 *   - CLKOUT 可以配置为 1 Hz，正好覆盖所有采样档位。
 *   - 1 Hz 边沿唤醒一次的开销约 0.3 ms（进 STOP + 恢复时钟），
 *     占空比 0.03%，对续航影响可忽略。
 *
 * 代价：即使没有任务要跑，每秒也会醒一次。若需要更长的睡眠，
 *      可以只在下次任务到期前打开 CLKOUT，其余时间关闭。
 */

#ifndef __BSP_WAKEUP_H
#define __BSP_WAKEUP_H

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 初始化 EXTI1 唤醒源（PB1，下降沿）
 * @return HAL_OK 表示初始化成功
 * @note  调用前需确保 GPIOB 时钟已开启。
 */
HAL_StatusTypeDef BSP_Wakeup_Init(void);

/**
 * @brief 自上次调用以来收到的唤醒边沿数
 */
uint32_t BSP_Wakeup_Count(void);

/**
 * @brief 清空计数
 */
void BSP_Wakeup_ResetCount(void);

#endif /* __BSP_WAKEUP_H */
