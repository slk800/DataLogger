/**
 * @file    bsp_rtc.c
 * @brief   PCF8563 实时时钟驱动实现
 * @author  SLK800
 * @date    2026-05
 * @version 1.0
 *
 * @note
 * PCF8563 的 BCD 编码规则：
 *   seconds : bit6..0 = 00..59 (BCD)，bit7 = VL
 *   minutes : bit6..0
 *   hours   : bit5..0
 *   days    : bit5..0
 *   weekdays: bit2..0
 *   months  : bit4..0，bit7 = century
 *   years   : 00..99
 */

#include "bsp_rtc.h"
#include "i2c.h"

/* ========== BCD 辅助 ========== */

static uint8_t BcdToBin(uint8_t bcd)
{
    return (uint8_t)(((bcd >> 4) * 10U) + (bcd & 0x0FU));
}

static uint8_t BinToBcd(uint8_t bin)
{
    return (uint8_t)(((bin / 10U) << 4) | (bin % 10U));
}

/* ========== 寄存器访问 ========== */

static HAL_StatusTypeDef Rtc_Read(uint8_t reg, uint8_t *buf, uint16_t len)
{
    if (buf == NULL || len == 0U)
    {
        return HAL_ERROR;
    }
    return HAL_I2C_Mem_Read(&hi2c1, PCF8563_I2C_ADDR, reg,
                            I2C_MEMADD_SIZE_8BIT, buf, len,
                            PCF8563_I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef Rtc_Write(uint8_t reg, const uint8_t *buf, uint16_t len)
{
    if (buf == NULL || len == 0U)
    {
        return HAL_ERROR;
    }
    return HAL_I2C_Mem_Write(&hi2c1, PCF8563_I2C_ADDR, reg,
                             I2C_MEMADD_SIZE_8BIT, (uint8_t *)buf, len,
                             PCF8563_I2C_TIMEOUT_MS);
}

/* ========== 公共接口 ========== */

HAL_StatusTypeDef BSP_RTC_Init(void)
{
    uint8_t ctrl = 0x00U;

    if (HAL_I2C_IsDeviceReady(&hi2c1, PCF8563_I2C_ADDR, 3U,
                              PCF8563_I2C_TIMEOUT_MS) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* CONTROL_1 = 0：正常模式，清除 TEST 位 */
    if (Rtc_Write(PCF8563_REG_CONTROL1, &ctrl, 1U) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* CONTROL_2 = 0：关闭报警中断、关闭定时器 */
    if (Rtc_Write(PCF8563_REG_CONTROL2, &ctrl, 1U) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* 默认 CLKOUT 关闭，需要时再打开，避免额外功耗 */
    return BSP_RTC_SetClkout(0U);
}

HAL_StatusTypeDef BSP_RTC_CheckClockLost(bool *lost)
{
    uint8_t sec = 0U;

    if (lost == NULL)
    {
        return HAL_ERROR;
    }

    if (Rtc_Read(PCF8563_REG_VL_SECONDS, &sec, 1U) != HAL_OK)
    {
        return HAL_ERROR;
    }

    *lost = ((sec & PCF8563_BIT_VL) != 0U);
    return HAL_OK;
}

HAL_StatusTypeDef BSP_RTC_GetTime(Pcf8563Time_t *out)
{
    uint8_t raw[7];

    if (out == NULL)
    {
        return HAL_ERROR;
    }

    /* 0x02..0x08 连续 7 字节：秒/分/时/日/星期/月/年 */
    if (Rtc_Read(PCF8563_REG_VL_SECONDS, raw, sizeof(raw)) != HAL_OK)
    {
        return HAL_ERROR;
    }

    out->second  = BcdToBin((uint8_t)(raw[0] & 0x7FU));
    out->minute  = BcdToBin((uint8_t)(raw[1] & 0x7FU));
    out->hour    = BcdToBin((uint8_t)(raw[2] & 0x3FU));
    out->day     = BcdToBin((uint8_t)(raw[3] & 0x3FU));
    out->weekday = (uint8_t)(raw[4] & 0x07U);
    out->month   = BcdToBin((uint8_t)(raw[5] & 0x1FU));
    out->year    = BcdToBin(raw[6]);

    /* 基本合法性检查：读出全 0 通常意味着芯片未初始化 */
    if (out->month < 1U || out->month > 12U ||
        out->day < 1U || out->day > 31U ||
        out->hour > 23U || out->minute > 59U || out->second > 59U)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

HAL_StatusTypeDef BSP_RTC_SetTime(const Pcf8563Time_t *t)
{
    uint8_t raw[7];

    if (t == NULL || t->month < 1U || t->month > 12U ||
        t->day < 1U || t->day > 31U || t->hour > 23U ||
        t->minute > 59U || t->second > 59U)
    {
        return HAL_ERROR;
    }

    raw[0] = BinToBcd(t->second) & 0x7FU;     /* 写 0 清零 VL 位 */
    raw[1] = BinToBcd(t->minute);
    raw[2] = BinToBcd(t->hour);
    raw[3] = BinToBcd(t->day);
    raw[4] = (uint8_t)(t->weekday & 0x07U);
    raw[5] = BinToBcd(t->month);              /* century 位保持 0（20xx） */
    raw[6] = BinToBcd(t->year);

    return Rtc_Write(PCF8563_REG_VL_SECONDS, raw, sizeof(raw));
}

HAL_StatusTypeDef BSP_RTC_SetClkout(uint16_t freq_hz)
{
    uint8_t reg;

    switch (freq_hz)
    {
    case 0U:       reg = 0x80U; break;   /* FE=1：关闭 */
    case 1U:       reg = 0x83U; break;   /* 1 Hz */
    case 32U:      reg = 0x82U; break;   /* 32 Hz */
    case 1024U:    reg = 0x81U; break;   /* 1024 Hz */
    case 32768U:   reg = 0x80U; break;   /* 32.768 kHz */
    default:       return HAL_ERROR;
    }

    return Rtc_Write(PCF8563_REG_CLKOUT, &reg, 1U);
}
