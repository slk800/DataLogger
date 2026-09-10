/**
 * @file    bsp_key.c
 * @brief   Active-low UP/DOWN/OK key scanning with debounce.
 */

#include "bsp_key.h"
#include "FreeRTOS.h"
#include "task.h"

#define KEY_DEBOUNCE_MS     30U
#define KEY_LONG_PRESS_MS   800U

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
    GPIO_PinState stable;
    GPIO_PinState last_raw;
    uint32_t last_change_ms;
    uint32_t press_start_ms;
    uint8_t long_reported;
} KeyState_t;

static KeyState_t s_keys[KEY_ID_COUNT] = {
    {KEY_UP_GPIO_Port, KEY_UP_Pin, GPIO_PIN_SET, GPIO_PIN_SET, 0U, 0U, 0U},
    {KEY_DOWN_GPIO_Port, KEY_DOWN_Pin, GPIO_PIN_SET, GPIO_PIN_SET, 0U, 0U, 0U},
    {KEY_OK_GPIO_Port, KEY_OK_Pin, GPIO_PIN_SET, GPIO_PIN_SET, 0U, 0U, 0U},
};

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
