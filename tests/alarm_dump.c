/*
 * 读取 stdin 的 "ts temp_x100" 序列，输出报警区间（与 dlreport.replay_alarm 对齐）。
 * 用法： ./alarm_dump <low> <high> <hyst> <min_dur_s> < sequence.txt
 */
#include <stdio.h>
#include <stdlib.h>
#include "domain_alarm.h"

int main(int argc, char **argv)
{
    AlarmCtx_t ctx;
    AlarmConfig_t cfg;
    AlarmEvent_t ev;
    unsigned ts;
    int temp;
    unsigned long t0 = 0;

    if (argc < 5) return 2;

    cfg.low_x100 = (int16_t)atoi(argv[1]);
    cfg.high_x100 = (int16_t)atoi(argv[2]);
    cfg.hyst_x100 = (int16_t)atoi(argv[3]);
    cfg.min_duration_s = (uint16_t)atoi(argv[4]);
    Alarm_Init(&ctx, &cfg);

    while (scanf("%u %d", &ts, &temp) == 2)
    {
        /* 用相对秒 ×1000 作为 ms 时间轴，与 PC 侧重放一致 */
        uint32_t now_ms = (uint32_t)((ts - t0) * 1000UL);
        AlarmEventType_e e = Alarm_Feed(&ctx, (int16_t)temp, now_ms, ts, &ev);
        if (e == ALARM_EVENT_CLEARED)
        {
            printf("%s %u %u %d %d\n",
                   ev.kind == ALARM_KIND_HIGH ? "HIGH" : "LOW",
                   ev.start_utc, ev.end_utc, ev.min_x100, ev.max_x100);
        }
    }
    return 0;
}
