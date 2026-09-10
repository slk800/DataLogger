/**
 * @file    ssd1306.c
 * @brief   SSD1306 128x64 monochrome OLED text driver.
 */

#include "ssd1306.h"
#include "font6x8.h"
#include <string.h>

#define SSD1306_TEXT_COLS  (SSD1306_WIDTH / FONT6X8_WIDTH)
#define SSD1306_TEXT_ROWS  SSD1306_PAGE_COUNT

static char s_text[SSD1306_TEXT_ROWS][SSD1306_TEXT_COLS];
static uint8_t s_page_buffer[SSD1306_WIDTH];
static uint8_t s_cursor_col;
static uint8_t s_cursor_y;
static uint8_t s_ready;

static HAL_StatusTypeDef SSD1306_WriteCommand(uint8_t cmd)
{
    uint8_t data[2] = {0x00, cmd};
    return BSP_OLED_I2C_Write(SSD1306_I2C_ADDR, data, sizeof(data));
}

static HAL_StatusTypeDef SSD1306_WriteData(const uint8_t *data, uint16_t len)
{
    uint8_t chunk[17];
    uint16_t offset = 0U;
    uint16_t n;

    while (offset < len)
    {
        n = (uint16_t)((len - offset) > 16U ? 16U : (len - offset));
        chunk[0] = 0x40;
        memcpy(&chunk[1], &data[offset], n);
        if (BSP_OLED_I2C_Write(SSD1306_I2C_ADDR, chunk, (uint16_t)(n + 1U)) != HAL_OK)
        {
            return HAL_ERROR;
        }
        offset += n;
    }

    return HAL_OK;
}

HAL_StatusTypeDef SSD1306_Init(void)
{
    static const uint8_t init_seq[] = {
        0xAE, 0x20, 0x00, 0xB0, 0xC8, 0x00, 0x10, 0x40,
        0x81, 0x7F, 0xA1, 0xA6, 0xA8, 0x3F, 0xA4, 0xD3,
        0x00, 0xD5, 0x80, 0xD9, 0xF1, 0xDA, 0x12, 0xDB,
        0x40, 0x8D, 0x14, 0xAF
    };
    uint8_t i;

    HAL_Delay(100);
    if (BSP_OLED_I2C_IsReady(SSD1306_I2C_ADDR) != HAL_OK)
    {
        s_ready = 0U;
        return HAL_ERROR;
    }

    for (i = 0U; i < (uint8_t)(sizeof(init_seq) / sizeof(init_seq[0])); i++)
    {
        if (SSD1306_WriteCommand(init_seq[i]) != HAL_OK)
        {
            s_ready = 0U;
            return HAL_ERROR;
        }
    }

    s_ready = 1U;
    SSD1306_Clear();
    return SSD1306_UpdateScreen();
}

uint8_t SSD1306_IsReady(void)
{
    return s_ready;
}

HAL_StatusTypeDef SSD1306_Recover(void)
{
    s_ready = 0U;
    (void)HAL_I2C_DeInit(&hi2c1);
    MX_I2C1_Init();
    return SSD1306_Init();
}

void SSD1306_Clear(void)
{
    memset(s_text, ' ', sizeof(s_text));
    s_cursor_col = 0U;
    s_cursor_y = 0U;
}

void SSD1306_SetCursor(uint8_t x, uint8_t y)
{
    s_cursor_col = (uint8_t)(x / FONT6X8_WIDTH);
    s_cursor_y = y;
}

void SSD1306_WriteChar(char ch)
{
    if (ch < FONT6X8_FIRST_CHAR || ch > FONT6X8_LAST_CHAR)
    {
        ch = '?';
    }
    if (s_cursor_col >= SSD1306_TEXT_COLS || s_cursor_y >= SSD1306_TEXT_ROWS)
    {
        return;
    }

    s_text[s_cursor_y][s_cursor_col] = ch;
    s_cursor_col++;
}

void SSD1306_WriteString(const char *str)
{
    while (str != NULL && *str != '\0')
    {
        SSD1306_WriteChar(*str++);
    }
}

void SSD1306_DrawStringAt(uint8_t x, uint8_t y, const char *str)
{
    SSD1306_SetCursor(x, y);
    SSD1306_WriteString(str);
}

HAL_StatusTypeDef SSD1306_UpdateScreen(void)
{
    uint8_t page;
    uint8_t col;
    uint8_t glyph_col;

    if (s_ready == 0U)
    {
        return HAL_ERROR;
    }

    for (page = 0U; page < SSD1306_PAGE_COUNT; page++)
    {
        memset(s_page_buffer, 0x00, sizeof(s_page_buffer));
        for (col = 0U; col < SSD1306_TEXT_COLS; col++)
        {
            char ch = s_text[page][col];
            const uint8_t *glyph;
            uint8_t x = (uint8_t)(col * FONT6X8_WIDTH);

            if (ch < FONT6X8_FIRST_CHAR || ch > FONT6X8_LAST_CHAR)
            {
                ch = '?';
            }
            glyph = g_font6x8[ch - FONT6X8_FIRST_CHAR];
            for (glyph_col = 0U; glyph_col < FONT6X8_WIDTH; glyph_col++)
            {
                s_page_buffer[x + glyph_col] = glyph[glyph_col];
            }
        }

        if (SSD1306_WriteCommand((uint8_t)(0xB0 + page)) != HAL_OK) goto fail;
        if (SSD1306_WriteCommand(0x00) != HAL_OK) goto fail;
        if (SSD1306_WriteCommand(0x10) != HAL_OK) goto fail;
        if (SSD1306_WriteData(s_page_buffer, SSD1306_WIDTH) != HAL_OK) goto fail;
    }

    return HAL_OK;

fail:
    s_ready = 0U;
    return HAL_ERROR;
}
