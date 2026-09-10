/**
 * @file    bsp_key.h
 * @brief   Three-key polling and debounce interface.
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

#endif /* __BSP_KEY_H */
