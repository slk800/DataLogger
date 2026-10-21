/**
 * @file    task_recorder.c
 * @brief   记录任务 - 时钟同步、双通道采样、报警与事件落盘
 * @author  SLK800
 * @date    2026-05
 * @version 1.0
 *
 * @note
 * 与 Task_DataProducer 的关系：
 *   DataProducer 负责"采一条、写一条样本"，本任务负责"采一条、判报警、
 *   写记录 v2、维护 MKT"。两者默认不同时开启（APP_ENABLE_RECORDER）。
 *
 * 选择本任务的原因：v2 记录需要墙钟时间、双通道和报警位，
 * 这些语义 DataProducer 的 v1 记录无法表达。保留 DataProducer 是为了
 * 让现有功能在不接 RTC 的板子上继续可用。
 */

#include "global.h"
#include "task_recorder.h"
#include "bsp_rtc.h"
#include "domain_time.h"
#include "domain_record.h"
#include "domain_alarm.h"
#include "domain_mkt.h"
#include "bsp_lowpower.h"
#include "bsp_wakeup.h"

#define RECORDER_FLASH_LOCK_MS  2000U
#define RECORDER_EVENT_FILE     "events"
#define RECORDER_EVENT_RESERVED (128UL * 1024UL)   /* 128 KB ≈ 8700 条事件 */

/* 事件记录布局：kind(1) + start(4) + end(4) + min(2) + max(2) + crc16(2) */
#define EVENT_RECORD_SIZE       15U

static uint32_t   s_utc;               /* 缓存的 UTC，0 = 未设置 */
static uint8_t    s_clock_lost;
static AlarmCtx_t s_alarm;
static MktCtx_t   s_mkt;
static uint32_t   s_event_count;
static uint8_t    s_seq;
static uint32_t   s_written;           /* 成功落盘的记录数（= 有效样本数） */
static int16_t    s_last_t1 = RECORD_TEMP_INVALID;
static int16_t    s_last_t2 = RECORD_TEMP_INVALID;
static uint16_t   s_vbat_mv;

/* ========== 时间 ========== */

static bool Recorder_SyncFromRtc(void)
{
    Pcf8563Time_t t;
    DateTime_t    dt;
    uint32_t      utc;
    bool          lost = false;

    if (BSP_RTC_GetTime(&t) != HAL_OK)
    {
        return false;
    }

    (void)BSP_RTC_CheckClockLost(&lost);
    if (lost)
    {
        /* VL=1：时钟掉过电，读数不可信，但先记下事实 */
        s_clock_lost = 1U;
    }

    dt.year   = (uint16_t)(2000U + t.year);
    dt.month  = t.month;
    dt.day    = t.day;
    dt.hour   = t.hour;
    dt.minute = t.minute;
    dt.second = t.second;

    if (!Time_IsValid(&dt))
    {
        return false;
    }

    utc = Time_ToUnix(&dt);
    if (utc != 0U)
    {
        s_utc = utc;
        return true;
    }
    return false;
}

uint32_t Recorder_GetUtc(void)
{
    return s_utc;
}

bool Recorder_SetUtc(uint32_t utc)
{
    DateTime_t dt;
    Pcf8563Time_t t;

    if (!Time_FromUnix(utc, &dt))
    {
        return false;
    }

    t.year    = (uint8_t)(dt.year % 100U);
    t.month   = dt.month;
    t.day     = dt.day;
    t.weekday = Time_Weekday(utc);
    t.hour    = dt.hour;
    t.minute  = dt.minute;
    t.second  = dt.second;

    if (BSP_RTC_SetTime(&t) != HAL_OK)
    {
        return false;
    }

    s_utc = utc;
    s_clock_lost = 0U;
    return true;
}

const AlarmCtx_t *Recorder_GetAlarm(void)
{
    return &s_alarm;
}

int16_t Recorder_GetMkt(void)
{
    return Mkt_Get(&s_mkt);
}

uint32_t Recorder_GetEventCount(void)
{
    return s_event_count;
}

/* ========== 事件落盘 ========== */

static uint16_t Event_CRC16(const uint8_t *p, uint32_t len)
{
    uint16_t crc = 0xFFFFU;
    uint32_t i;
    uint8_t  b;

    for (i = 0U; i < len; i++)
    {
        crc ^= (uint16_t)((uint16_t)p[i] << 8);
        for (b = 0U; b < 8U; b++)
        {
            crc = ((crc & 0x8000U) != 0U)
                ? (uint16_t)((uint16_t)(crc << 1) ^ 0x1021U)
                : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

static void PutU32LE(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
    p[2] = (uint8_t)((v >> 16) & 0xFFU);
    p[3] = (uint8_t)((v >> 24) & 0xFFU);
}

static void PutU16LE(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
}

static bool Recorder_AppendEvent(const AlarmEvent_t *ev)
{
    uint8_t  buf[EVENT_RECORD_SIZE];
    uint16_t crc;

    buf[0] = ev->kind;
    PutU32LE(&buf[1], ev->start_utc);
    PutU32LE(&buf[5], ev->end_utc);
    PutU16LE(&buf[9], (uint16_t)ev->min_x100);
    PutU16LE(&buf[11], (uint16_t)ev->max_x100);
    crc = Event_CRC16(buf, 13U);
    PutU16LE(&buf[13], crc);

    if (!BSP_Flash_Lock(RECORDER_FLASH_LOCK_MS))
    {
        return false;
    }

    /* 事件文件不存在则创建（首次运行时 Log_EnsureStorageFiles 只管 log/sample） */
    {
        FileEntry_t entry;
        if (FS_File_Find(RECORDER_EVENT_FILE, &entry, NULL) != FS_OK)
        {
            (void)FS_File_Create(RECORDER_EVENT_FILE, FILE_TYPE_EVENTS);
        }
    }

    {
        bool ok = (FS_File_AppendReserved(RECORDER_EVENT_FILE, buf, sizeof(buf),
                                          RECORDER_EVENT_RESERVED) == FS_OK);
        BSP_Flash_Unlock();
        if (ok)
        {
            s_event_count++;
        }
        return ok;
    }
}

/* ========== 主循环 ========== */

void Task_Recorder(void *pvParam)
{
    AlarmConfig_t acfg;
    uint32_t last_sync_ms;
    TickType_t last_tick;

    (void)pvParam;

    BSP_UART_SendString("[Recorder] started\r\n");

    /* 1. RTC */
    if (BSP_RTC_Init() != HAL_OK)
    {
        BSP_UART_SendString("[Recorder] RTC not found (time unavailable)\r\n");
        BSP_LowPower_Init(false);
    }
    else
    {
        (void)Recorder_SyncFromRtc();

        /*
         * 1.5 低功耗：只有检测到 RTC 才允许进 STOP。
         *     CLKOUT 1 Hz -> PB1 EXTI1，作为唯一的唤醒源。
         */
        (void)BSP_RTC_SetClkout(1U);
        (void)BSP_Wakeup_Init();
        BSP_LowPower_Init(true);
        BSP_UART_SendString("[Recorder] STOP mode enabled (CLKOUT 1Hz)\r\n");
    }

    /* 2. 报警配置来自 g_config（阈值字段在 W2 加入保留区） */
    acfg.low_x100       = (int16_t)g_config.alarm_low_x100;
    acfg.high_x100      = (int16_t)g_config.alarm_high_x100;
    acfg.hyst_x100      = (int16_t)g_config.alarm_hyst_x100;
    acfg.min_duration_s = (uint16_t)g_config.alarm_min_duration_s;
    Alarm_Init(&s_alarm, &acfg);
    Mkt_Init(&s_mkt);

    s_event_count = 0U;
    s_seq = 0U;
    last_sync_ms = xTaskGetTickCount();

    /* 固定周期基准：vTaskDelay 会让周期随 Flash 写入耗时漂移 */
    last_tick = xTaskGetTickCount();

    for (;;)
    {
        SensorNtcData_t ntc;
        RecordV2_t      rec;
        AlarmEvent_t    ev;
        AlarmEventType_e evt;
        uint32_t        now_ms;
        uint16_t        aux;

        now_ms = xTaskGetTickCount();

        /* 每 60 秒从 RTC 重新对一次时（I2C 读 7 字节，开销可忽略） */
        if ((now_ms - last_sync_ms) >= 60000U)
        {
            (void)Recorder_SyncFromRtc();
            last_sync_ms = now_ms;
        }

        (void)Sensor_NTC_Read(&ntc);

        /* 通道 1：真实 NTC；通道 2 未接时保持 INVALID */
        s_last_t1 = (ntc.status == SENSOR_NTC_OK)
                  ? (int16_t)(ntc.temperature_x10 * 10)
                  : RECORD_TEMP_INVALID;

        evt = Alarm_Feed(&s_alarm, s_last_t1, now_ms, s_utc, &ev);

        if (evt == ALARM_EVENT_STARTED)
        {
            (void)Recorder_AppendEvent(&ev);
            LOG_W("ALARM start kind=%u t=%d.%02d", ev.kind, ev.max_x100 / 100,
                  (ev.max_x100 < 0 ? -ev.max_x100 : ev.max_x100) % 100);
        }
        else if (evt == ALARM_EVENT_CLEARED)
        {
            (void)Recorder_AppendEvent(&ev);
            LOG_I("ALARM end kind=%u dur=%lus", ev.kind,
                  (unsigned long)(ev.end_utc - ev.start_utc));
        }

        if (s_last_t1 != RECORD_TEMP_INVALID)
        {
            (void)Mkt_Add(&s_mkt, s_last_t1);
        }

        aux = Alarm_AuxFlags(&s_alarm);

        rec.ts_utc  = s_utc;
        rec.t1_x100 = s_last_t1;
        rec.t2_x100 = s_last_t2;
        rec.vbat_mv = s_vbat_mv;
        rec.flags   = (uint8_t)(s_clock_lost ? RECORD_FLAG_CLOCK_LOST : 0U);
        rec.seq     = s_seq++;
        rec.aux     = aux;

        {
            uint8_t raw[RECORD_V2_SIZE];
            Record_Encode(&rec, raw);

            if (BSP_Flash_Lock(RECORDER_FLASH_LOCK_MS))
            {
                if (FS_File_AppendReserved(LOG_SAMPLE_FILE_NAME, raw, sizeof(raw),
                                           LOG_SAMPLE_RESERVED_BYTES) == FS_OK)
                {
                    s_written++;
                }
                BSP_Flash_Unlock();
            }
        }

        UI_Status_UpdateSensor(&ntc);
        UI_Status_UpdateProducer(s_written, ntc.temperature_x10, NULL);

        /*
         * vTaskDelayUntil 保证采样周期不受 Flash 写入耗时影响；
         * 若上一轮超时（例如擦除耗时），它会立即返回并重新对齐。
         */
        vTaskDelayUntil(&last_tick, pdMS_TO_TICKS(g_config.sample_interval_ms));
    }
}
