/**
 * @file    bsp_key.c
 * @brief   三键扫描：消抖、短按/长按事件与 EXTI 唤醒
 * @author  SLK800
 * @date    2026-05
 * @version 1.1
 *
 * @note
 * 按键为低电平有效（内部上拉，按下接 GND）。
 *
 * 两套机制并存：
 *   1. BSP_Key_Scan()  —— 50 ms 周期调用，负责消抖与事件判定
 *   2. EXTI 下降沿     —— 只置 s_wake_flag，把 MCU 从 STOP 唤醒
 *
 * 分开的原因：中断里做 30 ms 消抖会阻塞系统；而纯轮询在低功耗下
 * 每 50 ms 醒一次（占空比 4% × 15 mA ≈ 600 µA）又太贵。
 * 折中方案是"中断只负责叫醒，消抖仍在任务里做"。
 */

#include "bsp_key.h"
#include "FreeRTOS.h"
#include "task.h"

#define KEY_DEBOUNCE_MS     30U
#define KEY_LONG_PRESS_MS   800U
#define KEY_EXTI_PRIORITY   10U

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
    GPIO_PinState stable;
    GPIO_PinState last_raw;
    uint32_t last_change_ms;
    uint32_t press_start_ms;
    uint8_t long_reported;
} KeyState_t;

static volatile uint8_t s_wake_flag;

static KeyState_t s_keys[KEY_ID_COUNT] = {
    {KEY_UP_GPIO_Port, KEY_UP_Pin, GPIO_PIN_SET, GPIO_PIN_SET, 0U, 0U, 0U},
    {KEY_DOWN_GPIO_Port, KEY_DOWN_Pin, GPIO_PIN_SET, GPIO_PIN_SET, 0U, 0U, 0U},
    {KEY_OK_GPIO_Port, KEY_OK_Pin, GPIO_PIN_SET, GPIO_PIN_SET, 0U, 0U, 0U},
};

/**
 * @brief EXTI 回调：只置标志，不做消抖
 * @note  PB12/PB13/PB14 共用 EXTI15_10 中断线。
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    switch (GPIO_Pin)
    {
    case KEY_UP_Pin:
    case KEY_DOWN_Pin:
    case KEY_OK_Pin:
        s_wake_flag = 1U;
        break;
    default:
        break;
    }
}

void BSP_Key_Init(void)
{
    uint8_t i;
    uint32_t now = xTaskGetTickCount();

    for (i = 0U; i < KEY_ID_COUNT; i++)
    {
        s_keys[i].stable = HAL_GPIO_ReadPin(s_keys[i].port, s_keys[i].pin);
        s_keys[i].last_raw = s_keys[i].stable;
        s_keys[i].last_change_ms = now;
        s_keys[i].press_start_ms = 0U;
        s_keys[i].long_reported = 0U;
    }

    s_wake_flag = 0U;
    BSP_Key_EnableInterrupt(1U);
}

uint8_t BSP_Key_TakeWakeFlag(void)
{
    uint8_t flag = s_wake_flag;
    s_wake_flag = 0U;
    return flag;
}

void BSP_Key_EnableInterrupt(uint8_t enable)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = KEY_UP_Pin | KEY_DOWN_Pin | KEY_OK_Pin;
    gpio.Mode = enable ? GPIO_MODE_IT_FALLING : GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio);

    if (enable)
    {
        HAL_NVIC_SetPriority(EXTI15_10_IRQn, KEY_EXTI_PRIORITY, 0U);
        HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
    }
    else
    {
        HAL_NVIC_DisableIRQ(EXTI15_10_IRQn);
    }
}

static KeyEvent_e BSP_Key_MapShort(KeyId_e id)
{
    if (id == KEY_ID_UP)
    {
        return KEY_EVENT_UP_SHORT;
    }
    if (id == KEY_ID_DOWN)
    {
        return KEY_EVENT_DOWN_SHORT;
    }
    return KEY_EVENT_OK_SHORT;
}

KeyEvent_e BSP_Key_Scan(uint32_t now_ms)
{
    uint8_t i;

    for (i = 0U; i < KEY_ID_COUNT; i++)
    {
        KeyState_t *key = &s_keys[i];
        GPIO_PinState raw = HAL_GPIO_ReadPin(key->port, key->pin);

        if (raw != key->last_raw)
        {
            key->last_raw = raw;
            key->last_change_ms = now_ms;
        }

        if ((now_ms - key->last_change_ms) < KEY_DEBOUNCE_MS)
        {
            continue;
        }

        if (raw != key->stable)
        {
            key->stable = raw;
            if (raw == GPIO_PIN_RESET)
            {
                key->press_start_ms = now_ms;
                key->long_reported = 0U;
            }
            else if (key->long_reported == 0U)
            {
                return BSP_Key_MapShort((KeyId_e)i);
            }
        }

        if (key->stable == GPIO_PIN_RESET &&
            i == KEY_ID_OK &&
            key->long_reported == 0U &&
            (now_ms - key->press_start_ms) >= KEY_LONG_PRESS_MS)
        {
            key->long_reported = 1U;
            return KEY_EVENT_OK_LONG;
        }
    }

    return KEY_EVENT_NONE;
}
