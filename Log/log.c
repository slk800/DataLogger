/**
 * @file    log.c
 * @brief   日志模块实现 - 初始化、写入、等级过滤
 * @author  SLK800
 * @date    2026-05-01
 * @version 1.0
 */

#include "log.h"
#include "app_init.h"
#include "bsp_uart.h"
#include "fs_file.h"
#include "utils.h"
#include <stdio.h>
#include <stdarg.h>

/* ========== 全局变量定义 ========== */
LogLevel_e g_log_level  = LOG_LEVEL_INFO;
uint32_t   g_log_dropped = 0;

static QueueHandle_t s_LogQueue = NULL;
static volatile uint8_t s_sample_counter_reload = 0U;

static bool Log_RecoverSampleFile(void)
{
    SampleRecord_t sample;
    uint32_t size;
    uint32_t record_count;
    uint32_t valid_count = 0U;
    uint32_t offset;

    size = FS_File_GetSize(LOG_SAMPLE_FILE_NAME);
    if (size == 0U)
    {
        return true;
    }

    record_count = size / sizeof(SampleRecord_t);
    for (offset = 0U; offset < record_count; offset++)
    {
        if (FS_File_ReadRange(LOG_SAMPLE_FILE_NAME,
                              offset * sizeof(SampleRecord_t),
                              (uint8_t *)&sample,
                              sizeof(sample)) != FS_OK)
        {
            return false;
        }

        if (SampleRecord_IsBlank(&sample) || !SampleRecord_IsValid(&sample))
        {
            break;
        }
        valid_count++;
    }

    if (valid_count == 0U)
    {
        return false;
    }

    return FS_File_RepairPrefix(LOG_SAMPLE_FILE_NAME,
                                valid_count * sizeof(SampleRecord_t)) == FS_OK;
}

bool Log_EnsureStorageFiles(void)
{
    FileEntry_t entry;
    bool ok = true;

    if (FS_File_Find(LOG_TEXT_FILE_NAME, &entry, NULL) != FS_OK)
    {
        ok = (FS_File_Create(LOG_TEXT_FILE_NAME, FILE_TYPE_LOG) == FS_OK) && ok;
    }

    if (FS_File_Find(LOG_SAMPLE_FILE_NAME, &entry, NULL) != FS_OK)
    {
        ok = (FS_File_Create(LOG_SAMPLE_FILE_NAME, FILE_TYPE_DATA) == FS_OK) && ok;
    }

    return ok;
}

/* ========== 函数实现 ========== */

void Log_Init(void)
{
    g_log_level = LOG_LEVEL_INFO;

    /* 确保FS中存在"log"文件 */
    (void)Log_EnsureStorageFiles();
    (void)Log_RecoverSampleFile();

    s_LogQueue = xQueueCreate(16, sizeof(LogRecord_t));
}

void Log_Write(LogLevel_e level, const char *fmt, ...)
{
    LogRecord_t record;
    va_list args;
    int written;

    /* 1. 等级过滤 */
    if (level < g_log_level)
    {
        return;
    }

    /* 2. 填充LogRecord_t */
    record.timestamp = xTaskGetTickCount();
    record.level     = (uint8_t)level;

    va_start(args, fmt);
    written = vsnprintf(record.msg, sizeof(record.msg), fmt, args);
    va_end(args);

    if (written < 0)
    {
        record.msg[0] = '\0';
        record.len = 0U;
    }
    else if ((uint32_t)written >= sizeof(record.msg))
    {
        record.len = (uint8_t)(sizeof(record.msg) - 1U);
    }
    else
    {
        record.len = (uint8_t)written;
    }

    record.crc = Utils_CRC32_Calc((uint8_t *)record.msg, record.len);

    /* 3. 发送到队列 (超时50ms丢弃) */
    Log_Submit(&record);

    /* 4. 同时直接输出到串口 */
    BSP_UART_SendString(record.msg);
}

bool Log_Submit(const LogRecord_t *rec)
{
    if (s_LogQueue == NULL)
    {
        return false;
    }
    if (xQueueSend(s_LogQueue, rec, pdMS_TO_TICKS(50)) != pdTRUE)
    {
        g_log_dropped++;
        return false;
    }
    return true;
}

bool Log_Dequeue(LogRecord_t *rec, uint32_t timeout_ms)
{
    if (s_LogQueue == NULL)
    {
        return false;
    }
    return (xQueueReceive(s_LogQueue, rec, pdMS_TO_TICKS(timeout_ms)) == pdTRUE);
}

bool Log_AppendSample(const SampleRecord_t *sample)
{
    if (sample == NULL || !SampleRecord_IsValid(sample))
    {
        return false;
    }

    return (FS_File_AppendReserved(LOG_SAMPLE_FILE_NAME,
                                   (const uint8_t *)sample,
                                   sizeof(SampleRecord_t),
                                   LOG_SAMPLE_RESERVED_BYTES) == FS_OK);
}

uint32_t Log_CountValidSamples(void)
{
    SampleRecord_t sample;
    uint32_t size;
    uint32_t count;
    uint32_t offset;

    size = FS_File_GetSize(LOG_SAMPLE_FILE_NAME);
    count = size / sizeof(SampleRecord_t);

    for (offset = 0U; offset < count; offset++)
    {
        if (FS_File_ReadRange(LOG_SAMPLE_FILE_NAME,
                              offset * sizeof(SampleRecord_t),
                              (uint8_t *)&sample,
                              sizeof(sample)) != FS_OK)
        {
            return offset;
        }

        if (SampleRecord_IsBlank(&sample) || !SampleRecord_IsValid(&sample))
        {
            return offset;
        }
    }

    return count;
}

void Log_RequestSampleCounterReload(void)
{
    s_sample_counter_reload = 1U;
}

bool Log_TakeSampleCounterReloadRequest(void)
{
    if (s_sample_counter_reload == 0U)
    {
        return false;
    }

    s_sample_counter_reload = 0U;
    return true;
}
