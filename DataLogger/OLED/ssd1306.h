/**
 * @file    ssd1306.h
 * @brief   SSD1306 128x64 monochrome OLED text API.
 */

#ifndef __SSD1306_H
#define __SSD1306_H

#include "stm32f1xx_hal.h"
#include "bsp_oled_i2c.h"
#include <stdint.h>

#define SSD1306_WIDTH       128
#define SSD1306_HEIGHT      64
#define SSD1306_PAGE_COUNT  8
#define SSD1306_BUFFER_SIZE (SSD1306_WIDTH * SSD1306_HEIGHT / 8)
#define SSD1306_I2C_ADDR    OLED_I2C_ADDR_DEFAULT

typedef enum {
    SSD1306_COLOR_BLACK = 0,
    SSD1306_COLOR_WHITE = 1
} SSD1306_Color_e;

HAL_StatusTypeDef SSD1306_Init(void);
HAL_StatusTypeDef SSD1306_Recover(void);
void SSD1306_Clear(void);
void SSD1306_SetCursor(uint8_t x, uint8_t y);
void SSD1306_WriteChar(char ch);
void SSD1306_WriteString(const char *str);
void SSD1306_DrawStringAt(uint8_t x, uint8_t y, const char *str);
HAL_StatusTypeDef SSD1306_UpdateScreen(void);
uint8_t SSD1306_IsReady(void);

#endif /* __SSD1306_H */
