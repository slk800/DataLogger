/**
 * @file    task_idle_hook.c
 * @brief   FreeRTOS tickless 空闲钩子 - STOP 模式实现
 * @author  SLK800
 * @date    2026-05
 * @version 1.0
 *
 * @note
 * configUSE_TICKLESS_IDLE = 2 时，port.c 的
 * vPortSuppressTicksAndSleep 不参与编译（它只在 ==1 时提供），
 * 必须由应用实现 —— 这就是本文件存在的原因。
 *
 * 时序约束（本文件最重要的一段）：
 *   HAL_Delay / HAL_I2C_xxx / HAL_SPI_xxx 的超时都基于 SysTick。
 *   一旦 __disable_irq()，tick 不再前进，任何依赖 HAL 超时的调用都可能
 *   永久阻塞。因此**器件门控必须放在关中断之前**。
 *
 *   正确顺序：读时间基准 → 门控器件 → 关中断 → STOP
 *             → 开中断 → 恢复器件 → 补 tick → yield
 *
 * 睡眠时长的时间来源：
 *   STOP 模式下 SysTick 停止，只能用 RTC 秒计数测量。
 *   因此睡眠前读一次、醒来后读一次，用差值补 tick。
 *
 * 误差边界：
 *   PCF8563 分辨率 1 s。最长睡眠 60 s 时，补的 tick 最多少算 1 s。
 *   记录里的时间戳直接取自 RTC，不依赖 tick，因此**数据本身不受影响**；
 *   受影响的只是任务唤醒时刻（±1 s 抖动）。
 *
 * 为什么限制在 60 s：
 *   - 采样间隔默认 1000 ms，60 s 已远超任何任务的等待时间；
 *   - 过长的睡眠会让 RTC 读数误差累积，且不利于调试；
 *   - 需要更长周期时应改用 RTC alarm，而不是延长 STOP。
 */

#include "FreeRTOS.h"
#include "task.h"
#include "global.h"
#include "bsp_lowpower.h"
#include "task_recorder.h"

/*
 * port.c 在 portmacro.h 里只声明了这个函数的一部分用途，
 * ARMCC 会给出隐式声明告警。这里显式声明一次。
 */
extern void vPortSetupTimerInterrupt(void);

#if (configUSE_TICKLESS_IDLE == 2)

#define IDLE_MAX_SLEEP_TICKS    (TickType_t)(configTICK_RATE_HZ * 60UL)

void vPortSuppressTicksAndSleep(TickType_t xExpectedIdleTime)
{
    TickType_t target;
    uint32_t   before_utc;
    uint32_t   after_utc;
    TickType_t elapsed_ticks;
    BaseType_t yield_required;

    if (xExpectedIdleTime < configEXPECTED_IDLE_TIME_BEFORE_SLEEP)
    {
        return;
    }

    if (!BSP_LowPower_CanStop())
    {
        return;
    }

    target = xExpectedIdleTime;
    if (target > IDLE_MAX_SLEEP_TICKS)
    {
        target = IDLE_MAX_SLEEP_TICKS;
    }

    before_utc = Recorder_GetUtc();

    /*
     * 器件门控必须在中段之前完成：
     * Flash 深睡指令走 SPI，虽然不依赖 HAL_Delay，但 OLED 的 I2C 写
     * 有 HAL 超时；一旦关了中断，tick 冻结，超时永远不会到期。
     */
    BSP_LowPower_DeviceGate(true);

    /*
     * 与 port.c 的 tickless 实现保持相同的临界区语义：
     * 关中断 → 确认没有待处理切换 → 睡眠 → 开中断。
     * 这里用 vTaskSuspendAll 会引入调度器状态，反而更危险。
     */
    __disable_irq();

    if (eTaskConfirmSleepModeStatus() == eAbortSleep)
    {
        __enable_irq();
        BSP_LowPower_DeviceGate(false);
        return;
    }

    /*
     * 允许中断，让 EXTI（PCF8563 CLKOUT）能唤醒 MCU。
     * WFI 本身不需要关中断：在开中断状态下进入，任何中断都会唤醒它。
     */
    __enable_irq();

    (void)BSP_LowPower_EnterStop();

    /*
     * 醒来后先恢复器件，再关中断补 tick。
     * Flash 唤醒内部有 HAL_Delay(1)，必须在中断开启时执行。
     */
    BSP_LowPower_DeviceGate(false);

    __disable_irq();

    /*
     * 重装 SysTick：STOP 唤醒后 SysTick 的当前值已不可信，
     * 用 port.c 的初始化函数重新装填 reload 并启动。
     */
    vPortSetupTimerInterrupt();

    after_utc = Recorder_GetUtc();

    if (before_utc != 0U && after_utc > before_utc)
    {
        uint32_t elapsed_s = after_utc - before_utc;
        elapsed_ticks = (TickType_t)(elapsed_s * configTICK_RATE_HZ);
    }
    else
    {
        /* RTC 不可读或未设置：保守地按预期时长补 tick，绝不让时间倒退 */
        elapsed_ticks = target;
    }

    if (elapsed_ticks > target)
    {
        elapsed_ticks = target;
    }
    if (elapsed_ticks > 0U)
    {
        vTaskStepTick(elapsed_ticks);
    }

    /*
     * 补 tick 后如果有任务已到期，必须让出 CPU。
     *
     * 注意：这里不能再用 eTaskConfirmSleepModeStatus() —— 它只能在
     * 睡眠前调用一次。正确做法是检查补 tick 后是否有任务被解除阻塞：
     * vTaskStepTick 内部已有断言保证不会越过 xNextTaskUnblockTime，
     * 因此只需在开中断后主动 yield，让调度器重新评估就绪队列。
     */
    yield_required = (elapsed_ticks > 0U) ? pdTRUE : pdFALSE;
    __enable_irq();

    if (yield_required != pdFALSE)
    {
        portYIELD();
    }
}

#endif /* configUSE_TICKLESS_IDLE == 2 */
