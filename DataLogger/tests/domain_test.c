/*
 * 领域层主机侧测试：时间换算、记录编解码、报警状态机、MKT。
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "domain_time.h"
#include "domain_record.h"
#include "domain_alarm.h"
#include "domain_mkt.h"

static int failures;

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s  (line %d)\n", msg, __LINE__); failures++; } \
    else         { printf("ok  : %s\n", msg); } \
} while (0)

/* ---------------- 时间 ---------------- */
static void test_time(void)
{
    DateTime_t dt;
    uint32_t   ts;

    CHECK(Time_IsLeapYear(2000), "2000 is leap");
    CHECK(!Time_IsLeapYear(1900), "1900 is not leap");
    CHECK(Time_IsLeapYear(2024), "2024 is leap");
    CHECK(!Time_IsLeapYear(2026), "2026 is not leap");
    CHECK(Time_DaysInMonth(2024, 2) == 29, "Feb 2024 has 29 days");
    CHECK(Time_DaysInMonth(2026, 2) == 28, "Feb 2026 has 28 days");

    /* 已知时间点 */
    dt = (DateTime_t){1970, 1, 1, 0, 0, 0};
    CHECK(Time_ToUnix(&dt) == 0U, "epoch -> 0");

    dt = (DateTime_t){2000, 1, 1, 0, 0, 0};
    CHECK(Time_ToUnix(&dt) == 946684800UL, "2000-01-01 -> 946684800");

    dt = (DateTime_t){2026, 10, 21, 12, 34, 56};
    ts = Time_ToUnix(&dt);
    {
        DateTime_t back;
        CHECK(Time_FromUnix(ts, &back), "from unix ok");
        CHECK(back.year == 2026 && back.month == 10 && back.day == 21 &&
              back.hour == 12 && back.minute == 34 && back.second == 56,
              "unix roundtrip 2026-10-21 12:34:56");
    }

    /* 闰日边界 */
    dt = (DateTime_t){2024, 2, 29, 23, 59, 59};
    ts = Time_ToUnix(&dt);
    CHECK(ts > 0U, "leap day valid");
    {
        DateTime_t back;
        CHECK(Time_FromUnix(ts, &back) && back.day == 29 && back.month == 2,
              "leap day roundtrip");
        CHECK(Time_FromUnix(ts + 1U, &back) && back.month == 3 && back.day == 1,
              "leap day +1s -> Mar 1");
    }

    /* 非法时间 */
    dt = (DateTime_t){2026, 2, 29, 0, 0, 0};
    CHECK(!Time_IsValid(&dt), "2026-02-29 invalid");
    dt = (DateTime_t){2026, 13, 1, 0, 0, 0};
    CHECK(!Time_IsValid(&dt), "month 13 invalid");
    dt = (DateTime_t){2026, 1, 1, 24, 0, 0};
    CHECK(!Time_IsValid(&dt), "hour 24 invalid");

    /* 星期：2026-10-21 是星期三 */
    dt = (DateTime_t){2026, 10, 21, 0, 0, 0};
    CHECK(Time_Weekday(Time_ToUnix(&dt)) == 3U, "2026-10-21 is Wednesday");
    CHECK(Time_Weekday(0U) == 4U, "1970-01-01 is Thursday");
}

/* ---------------- 记录 ---------------- */
static void test_record(void)
{
    RecordV2_t rec = {
        .ts_utc = 1792000000UL,
        .t1_x100 = -1250,
        .t2_x100 = 2345,
        .vbat_mv = 3180,
        .flags = RECORD_FLAG_VBAT_LOW,
        .seq = 0x5A,
        .aux = RECORD_AUX_ALARM_HIGH,
    };
    uint8_t   buf[RECORD_V2_SIZE];
    RecordV2_t out;

    CHECK(sizeof(RecordV2_t) >= 14U, "struct fits payload");

    Record_Encode(&rec, buf);
    CHECK(Record_Decode(buf, &out), "decode valid record");
    CHECK(out.ts_utc == rec.ts_utc, "ts roundtrip");
    CHECK(out.t1_x100 == rec.t1_x100, "t1 roundtrip (negative)");
    CHECK(out.t2_x100 == rec.t2_x100, "t2 roundtrip");
    CHECK(out.vbat_mv == rec.vbat_mv, "vbat roundtrip");
    CHECK(out.flags == rec.flags, "flags roundtrip");
    CHECK(out.seq == rec.seq, "seq roundtrip");
    CHECK(out.aux == rec.aux, "aux roundtrip");

    /* 单比特翻转必须被检测 */
    {
        int bit;
        int detected = 0;
        for (bit = 0; bit < 14 * 8; bit++)
        {
            uint8_t bad[RECORD_V2_SIZE];
            memcpy(bad, buf, sizeof(bad));
            bad[bit / 8] ^= (uint8_t)(1U << (bit % 8));
            if (!Record_Decode(bad, &out))
            {
                detected++;
            }
        }
        CHECK(detected == 14 * 8, "all single-bit flips detected");
    }

    /* 擦除态 */
    memset(buf, 0xFF, sizeof(buf));
    CHECK(Record_IsBlank(buf), "erased page is blank");

    /* 温度量程 */
    CHECK(Record_TempValid(-4000) && Record_TempValid(12500), "temp range bounds");
    CHECK(!Record_TempValid(-4001) && !Record_TempValid(12501), "temp out of range");
}

/* ---------------- 报警 ---------------- */
static void test_alarm(void)
{
    AlarmCtx_t ctx;
    AlarmEvent_t ev;
    AlarmConfig_t cfg = {
        .low_x100 = 200,      /* 2.0 °C */
        .high_x100 = 800,     /* 8.0 °C */
        .hyst_x100 = 50,      /* 0.5 °C */
        .min_duration_s = 3,  /* 3 秒 */
    };
    uint32_t t_ms = 0U;
    uint32_t t_utc = 1000U;
    AlarmEventType_e e;

    /* 1. 正常区间，不应触发 */
    Alarm_Init(&ctx, &cfg);
    e = Alarm_Feed(&ctx, 500, t_ms, t_utc, &ev);
    CHECK(e == ALARM_EVENT_NONE && !Alarm_IsActive(&ctx), "normal temp stays normal");

    /* 2. 越上限 1 秒：未达最短持续时间 */
    e = Alarm_Feed(&ctx, 900, 1000U, 1001U, &ev);
    CHECK(e == ALARM_EVENT_NONE && !Alarm_IsActive(&ctx), "high for 1s not yet alarm");

    /* 3. 持续 3 秒：正式报警 */
    e = Alarm_Feed(&ctx, 900, 4000U, 1004U, &ev);
    CHECK(e == ALARM_EVENT_STARTED, "high for 3s starts alarm");
    CHECK(ev.kind == ALARM_KIND_HIGH, "event kind = HIGH");
    CHECK(Alarm_IsActive(&ctx), "alarm active");
    CHECK(Alarm_AuxFlags(&ctx) == (RECORD_AUX_ALARM_HIGH | RECORD_AUX_ALARM_ACTIVE),
          "aux flags reflect HIGH");

    /* 4. 回差区间内（8.0 -> 7.7，高于 7.5 退出阈值）仍保持报警 */
    e = Alarm_Feed(&ctx, 770, 5000U, 1005U, &ev);
    CHECK(e == ALARM_EVENT_NONE && Alarm_IsActive(&ctx),
          "within hysteresis band, alarm stays active");

    /* 5. 跌破退出阈值但未持续 3 秒 */
    e = Alarm_Feed(&ctx, 740, 6000U, 1006U, &ev);
    CHECK(e == ALARM_EVENT_NONE && Alarm_IsActive(&ctx),
          "below exit threshold for 1s, still active");

    /* 6. 持续 3 秒后解除 */
    e = Alarm_Feed(&ctx, 740, 9000U, 1009U, &ev);
    CHECK(e == ALARM_EVENT_CLEARED, "below exit threshold for 3s clears alarm");
    CHECK(ev.kind == ALARM_KIND_HIGH, "cleared event kind = HIGH");
    CHECK(ev.start_utc == 1001U, "event start_utc = first sample above threshold");
    /*
     * end_utc 取"首次回到退出阈值内侧"的时刻（1006），
     * 而不是消抖确认完成的时刻（1009）——报警区间不应包含消抖时长。
     */
    printf("      (actual end_utc=%u)\n", ev.end_utc);
    CHECK(ev.end_utc == 1006U, "event end_utc = first sample back inside");
    CHECK(ev.max_x100 == 900, "event max recorded");
    CHECK(!Alarm_IsActive(&ctx), "alarm inactive after clear");

    /* 7. 低温方向 */
    Alarm_Init(&ctx, &cfg);
    Alarm_Feed(&ctx, 100, 0U, 2000U, &ev);
    e = Alarm_Feed(&ctx, 100, 3000U, 2003U, &ev);
    CHECK(e == ALARM_EVENT_STARTED && ev.kind == ALARM_KIND_LOW, "low alarm starts");

    /* 8. 抖动不应产生报警（每次只越限 1 秒） */
    Alarm_Init(&ctx, &cfg);
    {
        int i;
        int started = 0;
        for (i = 0; i < 20; i++)
        {
            if (Alarm_Feed(&ctx, 900, (uint32_t)i * 2000U, (uint32_t)i, &ev)
                == ALARM_EVENT_STARTED)
            {
                started++;
            }
            if (Alarm_Feed(&ctx, 500, (uint32_t)i * 2000U + 1000U, (uint32_t)i, &ev)
                == ALARM_EVENT_STARTED)
            {
                started++;
            }
        }
        CHECK(started == 0, "flapping temp never starts alarm");
    }

    /* 9. 非法温度不推进状态机 */
    Alarm_Init(&ctx, &cfg);
    CHECK(Alarm_Feed(&ctx, RECORD_TEMP_INVALID, 0U, 0U, &ev) == ALARM_EVENT_NONE,
          "invalid temp ignored");
    CHECK(Alarm_Feed(&ctx, 900, 0U, 0U, &ev) == ALARM_EVENT_NONE,
          "invalid temp does not start candidate");
}

/* ---------------- MKT ---------------- */
static void test_mkt(void)
{
    MktCtx_t ctx;

    /* 恒定温度：MKT 应等于该温度 */
    Mkt_Init(&ctx);
    for (int i = 0; i < 100; i++)
    {
        Mkt_Add(&ctx, 500);     /* 5.00 °C */
    }
    CHECK(Mkt_Count(&ctx) == 100U, "MKT count");
    CHECK(abs((int)Mkt_Get(&ctx) - 500) <= 1, "constant temp -> same MKT");

    /* 波动温度：MKT 应高于算术平均值（因为指数量化了高温的影响） */
    Mkt_Init(&ctx);
    for (int i = 0; i < 50; i++) Mkt_Add(&ctx, 0);      /* 0 °C */
    for (int i = 0; i < 50; i++) Mkt_Add(&ctx, 1000);   /* 10 °C */
    {
        int16_t mkt = Mkt_Get(&ctx);
        int     arithmetic = 500;   /* 5.00 °C */
        CHECK(mkt > arithmetic, "MKT > arithmetic mean for oscillating temp");
        CHECK(mkt < 1000, "MKT < max temp");
        printf("      (MKT = %.2f C, arithmetic = 5.00 C)\n", mkt / 100.0);
    }

    /* 空上下文 */
    Mkt_Init(&ctx);
    CHECK(Mkt_Get(&ctx) == 0, "empty MKT returns 0");

    /* 非法样本 */
    CHECK(!Mkt_Add(&ctx, RECORD_TEMP_INVALID), "invalid temp rejected");
    CHECK(Mkt_Count(&ctx) == 0U, "invalid temp not counted");
}

int main(void)
{
    test_time();
    test_record();
    test_alarm();
    test_mkt();

    printf("\ndomain_test: %s (%d failures)\n", failures ? "FAILED" : "PASSED", failures);
    return failures ? 1 : 0;
}
