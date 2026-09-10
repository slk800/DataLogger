/**
 * @file    log.h
 * @brief   日志模块 - 日志记录结构体、等级定义与写入接口
 * @author  SLK800
 * @date    2026-05-01
 * @version 1.0
 */

#ifndef __LOG_H
#define __LOG_H

#include "global.h"
#include <stdarg.h>
#include <stdbool.h>

/* ========== 日志等级枚举 ========== */
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO  = 1,
    LOG_LEVEL_WARN  = 2,
    LOG_LEVEL_ERROR = 3
} LogLevel_e;

/* ========== 日志记录结构体 (80字节) ========== */
typedef struct {
    uint32_t  timestamp;       /* xTaskGetTickCount() 值 */
    uint8_t   level;           /* LogLevel_e 枚举值 */
    uint8_t   len;             /* msg 实际长度 */
    uint8_t   reserved[2];     /* 对齐 */
    char      msg[68];         /* 格式化后的日志内容 */
    uint32_t  crc;             /* 对 msg 前 len 字节的 CRC32 */
} LogRecord_t;

/* ========== 短文件名宏 ========== */
#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)

/* ========== 日志写入宏 ========== */
#define LOG_D(fmt, ...) Log_Write(LOG_LEVEL_DEBUG, "[D][%s:%d] " fmt "\r\n", __FILENAME__, __LINE__, ##__VA_ARGS__)
#define LOG_I(fmt, ...) Log_Write(LOG_LEVEL_INFO,  "[I] " fmt "\r\n", ##__VA_ARGS__)
#define LOG_W(fmt, ...) Log_Write(LOG_LEVEL_WARN,  "[W][%s:%d] " fmt "\r\n", __FILENAME__, __LINE__, ##__VA_ARGS__)
#define LOG_E(fmt, ...) Log_Write(LOG_LEVEL_ERROR, "[E][%s:%d] " fmt "\r\n", __FILENAME__, __LINE__, ##__VA_ARGS__)

/* ========== 全局变量声明 ========== */
extern LogLevel_e g_log_level;
#define LOG_TEXT_FILE_NAME      "log"
#define LOG_SAMPLE_FILE_NAME    "sample"
#define LOG_SAMPLE_RESERVED_BYTES (2UL * 1024UL * 1024UL)
extern uint32_t  g_log_dropped;     /* 因队列满而丢弃的日志数 */

/* ========== 函数声明 ========== */

/**
 * @brief 日志模块初始化
 * @note  设置默认等级INFO，确保FS中存在"log"文件
 */
void Log_Init(void);

void Log_Write(LogLevel_e level, const char *fmt, ...);
bool Log_Submit(const LogRecord_t *rec);
bool Log_Dequeue(LogRecord_t *rec, uint32_t timeout_ms);
bool Log_EnsureStorageFiles(void);
bool Log_AppendSample(const SampleRecord_t *sample);
uint32_t Log_CountValidSamples(void);
void Log_RequestSampleCounterReload(void);
bool Log_TakeSampleCounterReloadRequest(void);

#endif /* __LOG_H */
