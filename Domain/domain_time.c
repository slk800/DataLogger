/**
 * @file    domain_time.c
 * @brief   UTC 时间与 Unix 秒互转
 * @author  SLK800
 * @date    2026-05
 * @version 1.0
 *
 * @note
 * 用"从 1970 年起逐日累加"的方式实现，代码比 Howard Hinnant 的
 * days_from_civil 稍长，但不依赖除法技巧，在 Cortex-M3 上完全够用，
 * 并且每一行都能被单测覆盖。
 */

#include "domain_time.h"

bool Time_IsLeapYear(uint16_t year)
{
    if ((year % 400U) == 0U)
    {
        return true;
    }
    if ((year % 100U) == 0U)
    {
        return false;
    }
    return (year % 4U) == 0U;
}

uint8_t Time_DaysInMonth(uint16_t year, uint8_t month)
{
    static const uint8_t days[12] = {31, 28, 31, 30, 31, 30,
                                     31, 31, 30, 31, 30, 31};

    if (month < 1U || month > 12U)
    {
        return 0U;
    }
    if (month == 2U && Time_IsLeapYear(year))
    {
        return 29U;
    }
    return days[month - 1U];
}

bool Time_IsValid(const DateTime_t *dt)
{
    uint8_t max_day;

    if (dt == NULL)
    {
        return false;
    }
    if (dt->year < 1970U || dt->year > 2105U)
    {
        return false;
    }
    if (dt->month < 1U || dt->month > 12U)
    {
        return false;
    }

    max_day = Time_DaysInMonth(dt->year, dt->month);
    if (dt->day < 1U || dt->day > max_day)
    {
        return false;
    }

    return (dt->hour < 24U) && (dt->minute < 60U) && (dt->second < 60U);
}

uint32_t Time_ToUnix(const DateTime_t *dt)
{
    uint32_t days = 0U;
    uint16_t y;
    uint8_t  m;

    if (!Time_IsValid(dt))
    {
        return 0U;
    }

    for (y = 1970U; y < dt->year; y++)
    {
        days += Time_IsLeapYear(y) ? 366U : 365U;
    }
    for (m = 1U; m < dt->month; m++)
    {
        days += Time_DaysInMonth(dt->year, m);
    }
    days += (uint32_t)(dt->day - 1U);

    return days * 86400UL
         + (uint32_t)dt->hour * 3600UL
         + (uint32_t)dt->minute * 60UL
         + (uint32_t)dt->second;
}

bool Time_FromUnix(uint32_t unix_sec, DateTime_t *out)
{
    uint32_t days;
    uint32_t rem;
    uint16_t year = 1970U;
    uint8_t  month = 1U;
    uint8_t  dim;
    uint32_t year_days;

    if (out == NULL)
    {
        return false;
    }

    /* 2105-12-31 之后的秒数不接受（与 Time_IsValid 的上限一致） */
    if (unix_sec > 4291833599UL)
    {
        return false;
    }

    days = unix_sec / 86400UL;
    rem  = unix_sec % 86400UL;

    for (;;)
    {
        year_days = Time_IsLeapYear(year) ? 366U : 365U;
        if (days < year_days)
        {
            break;
        }
        days -= year_days;
        year++;
    }

    for (;;)
    {
        dim = Time_DaysInMonth(year, month);
        if (days < (uint32_t)dim)
        {
            break;
        }
        days -= dim;
        month++;
    }

    out->year   = year;
    out->month  = month;
    out->day    = (uint8_t)(days + 1U);
    out->hour   = (uint8_t)(rem / 3600UL);
    out->minute = (uint8_t)((rem % 3600UL) / 60UL);
    out->second = (uint8_t)(rem % 60UL);

    return true;
}

uint8_t Time_Weekday(uint32_t unix_sec)
{
    /* 1970-01-01 是星期四 */
    return (uint8_t)(((unix_sec / 86400UL) + 4UL) % 7UL);
}
