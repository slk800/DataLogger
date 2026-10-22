/**
 * @file    bsp_wakeup.c
 * @brief   PCF8563 CLKOUT 唤醒源实现
 * @author  SLK800
 * @date    2026-05
 * @version 1.0
 *
 * @note
 * EXTI1 的优先级必须低于 configMAX_SYSCALL_INTERRUPT_PRIORITY（数值更大），
 * 否则 FreeRTOS 会在中断里断言。这里用 10，低于临界值 5。
 *
 * 不使用 HAL 的 EXTI 框架（HAL_GPIO_EXTI_Callback），
 * 因为唤醒路径越短越好：直接读状态、计数、清标志即可。
 */

#include "bsp_wakeup.h"

#define WAKEUP_EXTI_LINE        EXTI_Line1
#define WAKEUP_IRQn             EXTI1_IRQn
#define WAKEUP_IRQ_PRIORITY     10U

static volatile uint32_t s_count;

HAL_StatusTypeDef BSP_Wakeup_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    /* PB1 输入 + 上拉：CLKOUT 是开漏输出，需要外部上拉 */
    __HAL_RCC_GPIOB_CLK_ENABLE();

    gpio.Pin = GPIO_PIN_1;
    gpio.Mode = GPIO_MODE_IT_FALLING;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio);

    /* 清掉可能的挂起标志后再使能，避免上一次残留触发 */
    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_1);

    HAL_NVIC_SetPriority(WAKEUP_IRQn, WAKEUP_IRQ_PRIORITY, 0U);
    HAL_NVIC_EnableIRQ(WAKEUP_IRQn);

    s_count = 0U;
    return HAL_OK;
}

uint32_t BSP_Wakeup_Count(void)
{
    return s_count;
}

void BSP_Wakeup_ResetCount(void)
{
    s_count = 0U;
}

/**
 * @brief EXTI1 中断服务程序
 * @note  文件名与本函数名需要和启动文件里的向量表一致。
 *        CubeMX 生成的 stm32f1xx_it.c 没有定义 EXTI1_IRQHandler，
 *        因此这里提供强符号，不会冲突。
 */
void EXTI1_IRQHandler(void)
{
    if (__HAL_GPIO_EXTI_GET_IT(GPIO_PIN_1) != RESET)
    {
        __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_1);
        s_count++;
    }
    /* 不要在这里调用任何 FreeRTOS API：唤醒路径要求最短 */
}
