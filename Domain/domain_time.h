/**
 * @file    domain_time.h
 * @brief   UTC 时间与 Unix 秒互转（纯逻辑，无 HAL 依赖）
 * @author  SLK800
 * @date    2026-05
 * @version 1.0
 *
 * @note
 * 不处理闰秒，不考虑 2100 年之后的闰年规则。
 * 设备时钟由 PCF8563 提供（本地 BCD 时间），写入记录前统一转成
 * "自 1970-01-01 00:00:00 UTC 起的秒数"。
 */

#ifndef __DOMAIN_TIME_H
#define __DOMAIN_TIME_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    uint16_t year;      /* 完整年份，例如 2026 */
    uint8_t  month;     /* 1..12 */
    uint8_t  day;       /* 1..31 */
    uint8_t  hour;      /* 0..23 */
    uint8_t  minute;    /* 0..59 */
    uint8_t  second;    /* 0..59 */
} DateTime_t;

/**
 * @brief 判断是否为闰年
 */
bool Time_IsLeapYear(uint16_t year);

/**
 * @brief 某年某月的天数
 * @return 0 表示月份非法
 */
uint8_t Time_DaysInMonth(uint16_t year, uint8_t month);

/**
 * @brief 校验日期时间是否合法
 */
bool Time_IsValid(const DateTime_t *dt);

/**
 * @brief DateTime -> Unix 秒
 * @return 0 表示参数非法
 * @note  1970-01-01 00:00:00 返回 0，因此 0 同时表示"未设置"。
 */
uint32_t Time_ToUnix(const DateTime_t *dt);

/**
 * @brief Unix 秒 -> DateTime
 * @return false 表示时间超出可表示范围
 */
bool Time_FromUnix(uint32_t unix_sec, DateTime_t *out);

/**
 * @brief 星期（0=周日），用于 PCF8563 的 weekday 字段
 */
uint8_t Time_Weekday(uint32_t unix_sec);

#endif /* __DOMAIN_TIME_H */
