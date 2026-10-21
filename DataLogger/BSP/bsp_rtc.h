/**
 * @file    bsp_rtc.h
 * @brief   PCF8563 实时时钟驱动（I2C，与 OLED 共用 I2C1）
 * @author  SLK800
 * @date    2026-05
 * @version 1.0
 *
 * @note
 * 为什么用 PCF8563 而不是 STM32 内部 RTC：
 *   - 内部 RTC 需要 32.768 kHz 晶振 + VBAT 电池，Blue Pill 上需要焊接；
 *   - PCF8563 模块自带晶振和电池座，只需 4 根杜邦线（VCC/GND/SDA/SCL）；
 *   - PCF8563 工作电流约 0.25 µA，远低于 DS3231 的 110–200 µA。
 *
 * 硬件接线：
 *   VCC  -> 3.3V
 *   GND  -> GND
 *   SDA  -> PB7（与 OLED 共用）
 *   SCL  -> PB6（与 OLED 共用）
 *   CLKOUT -> PB1（可选，用于低功耗唤醒，W3 再启用）
 *
 * 地址：0x51（7 位），I2C1 上 OLED(0x3C) 与 PCF8563(0x51) 不冲突。
 */

#ifndef __BSP_RTC_H
#define __BSP_RTC_H

#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

#define PCF8563_I2C_ADDR        (0x51 << 1)
#define PCF8563_I2C_TIMEOUT_MS  50U

/* 寄存器地址 */
#define PCF8563_REG_CONTROL1    0x00
#define PCF8563_REG_CONTROL2    0x01
#define PCF8563_REG_VL_SECONDS  0x02
#define PCF8563_REG_MINUTES     0x03
#define PCF8563_REG_HOURS       0x04
#define PCF8563_REG_DAYS        0x05
#define PCF8563_REG_WEEKDAYS    0x06
#define PCF8563_REG_MONTHS      0x07
#define PCF8563_REG_YEARS       0x08
#define PCF8563_REG_MIN_ALARM   0x09
#define PCF8563_REG_HOUR_ALARM  0x0A
#define PCF8563_REG_DAY_ALARM   0x0B
#define PCF8563_REG_WEEK_ALARM  0x0C
#define PCF8563_REG_CLKOUT      0x0D
#define PCF8563_REG_TIMER_CTRL  0x0E
#define PCF8563_REG_TIMER       0x0F

/* 状态 */
#define PCF8563_BIT_VL          (1U << 7)   /* 秒寄存器 bit7：低电压/时钟丢失 */
#define PCF8563_BIT_CENTURY     (1U << 7)   /* 月寄存器 bit7：世纪 */

typedef struct {
    uint8_t  year;      /* 00..99 */
    uint8_t  month;     /* 1..12 */
    uint8_t  day;       /* 1..31 */
    uint8_t  weekday;   /* 0..6 */
    uint8_t  hour;      /* 0..23 */
    uint8_t  minute;    /* 0..59 */
    uint8_t  second;    /* 0..59 */
} Pcf8563Time_t;

/**
 * @brief 初始化总线访问并检查器件
 * @return HAL_OK 表示器件应答
 */
HAL_StatusTypeDef BSP_RTC_Init(void);

/**
 * @brief 检测上一次是否发生过掉电（VL 位）
 * @param lost 输出：true 表示时钟曾丢失
 */
HAL_StatusTypeDef BSP_RTC_CheckClockLost(bool *lost);

/**
 * @brief 读取当前时间
 * @return HAL_OK 成功；读取失败返回 HAL_ERROR
 * @note  读到秒寄存器的 VL 位时会把 clock_lost 置位，但时间仍返回。
 */
HAL_StatusTypeDef BSP_RTC_GetTime(Pcf8563Time_t *out);

/**
 * @brief 设置时间
 */
HAL_StatusTypeDef BSP_RTC_SetTime(const Pcf8563Time_t *t);

/**
 * @brief 配置 CLKOUT 输出频率
 * @param freq_hz 0=关闭, 1, 32, 1024, 32768
 * @note  默认 1 Hz，用于低功耗唤醒（W3）。
 */
HAL_StatusTypeDef BSP_RTC_SetClkout(uint16_t freq_hz);

#endif /* __BSP_RTC_H */
