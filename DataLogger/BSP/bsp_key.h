/**
 * @file    bsp_key.h
 * @brief   三键扫描：消抖、短按/长按事件与 EXTI 唤醒
 * @author  SLK800
 * @date    2026-05
 * @version 1.1
 */

#ifndef __BSP_KEY_H
#define __BSP_KEY_H

#include "main.h"
#include <stdint.h>

typedef enum {
    KEY_ID_UP = 0,
    KEY_ID_DOWN,
    KEY_ID_OK,
    KEY_ID_COUNT
} KeyId_e;

typedef enum {
    KEY_EVENT_NONE = 0,
    KEY_EVENT_UP_SHORT,
    KEY_EVENT_DOWN_SHORT,
    KEY_EVENT_OK_SHORT,
    KEY_EVENT_OK_LONG
} KeyEvent_e;

void BSP_Key_Init(void);
KeyEvent_e BSP_Key_Scan(uint32_t now_ms);

/**
 * @brief 是否有按键边沿触发了唤醒
 * @note  EXTI 中断只置位标志，真正的消抖与事件判定仍在 BSP_Key_Scan 中，
 *        这样采样时序和轮询版完全一致，只是不再需要 50 ms 轮询。
 */
uint8_t BSP_Key_TakeWakeFlag(void);

/**
 * @brief 使能/禁用按键 EXTI
 * @param enable 1 = 下降沿中断，0 = 普通输入
 */
void BSP_Key_EnableInterrupt(uint8_t enable);

#endif /* __BSP_KEY_H */
