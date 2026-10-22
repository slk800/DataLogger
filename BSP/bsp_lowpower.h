/**
 * @file    bsp_lowpower.h
 * @brief   STOP 模式低功耗支持 - 进入/退出与时钟恢复
 * @author  SLK800
 * @date    2026-05
 * @version 1.0
 *
 * @note
 * STM32F103 在 STOP 模式下：
 *   - 所有时钟停止（HSE/HSI/PLL 全关）
 *   - SRAM 与寄存器内容保持
 *   - 只能被 EXTI（含 RTC 中断）或 NRST 唤醒
 *   - 唤醒后系统时钟回到 HSI 8 MHz，必须重新配置 PLL
 *
 * 本模块只负责"进入 STOP"和"唤醒后恢复时钟"，
 * 何时进入由 FreeRTOS 的 tickless 空闲钩子决定（见 App/task_idle_hook.c）。
 *
 * 低功耗的三个层级（缺一层就达不到目标）：
 *   1. 软件：tickless + STOP + 外设门控          ← 本模块
 *   2. 板级：电源 LED（1.5–2.5 mA）、AMS1117（3–5 mA）  ← 只能靠硬件处理
 *   3. 器件：OLED 休眠、W25Q64 深睡、传感器断电   ← 驱动层配合
 */

#ifndef __BSP_LOWPOWER_H
#define __BSP_LOWPOWER_H

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 初始化低功耗支持
 * @param rtc_available 是否存在可用的 RTC 唤醒源
 * @note  没有 RTC 时禁用 STOP（避免睡下去无法按时唤醒）。
 */
void BSP_LowPower_Init(bool rtc_available);

/**
 * @brief 是否允许进入 STOP
 */
bool BSP_LowPower_CanStop(void);

/**
 * @brief 进入 STOP 模式并等待唤醒（返回时时钟已恢复）
 * @return HAL_OK 表示时钟恢复成功
 */
HAL_StatusTypeDef BSP_LowPower_EnterStop(void);

/**
 * @brief 唤醒后恢复 72 MHz 系统时钟
 */
HAL_StatusTypeDef BSP_LowPower_RestoreClock(void);

/**
 * @brief 睡眠前的器件门控
 * @param sleep true = 进入睡眠，false = 唤醒后恢复
 * @note  OLED 休眠与 W25Q64 深睡由各自驱动负责，这里只协调调用顺序。
 */
void BSP_LowPower_DeviceGate(bool sleep);

/**
 * @brief 查询当前是否处于低功耗模式
 */
bool BSP_LowPower_IsActive(void);

#endif /* __BSP_LOWPOWER_H */
