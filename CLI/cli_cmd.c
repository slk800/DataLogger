/**
 * @file    cli_cmd.c
 * @brief   CLI命令实现 - help/ls/cat/rm/erase/status/task/heap/set/get/save/loglvl/reset
 * @author  SLK800
 * @date    2026-05-01
 * @version 1.0
 */

#include "cli_cmd.h"
#include "cli.h"
#include "log.h"
#include "config.h"
#include "app_init.h"
#include "bsp_uart.h"
#include "bsp_flash.h"
#include "fs_core.h"
#include "fs_file.h"
#include "utils.h"
#include "task_recorder.h"
#include "i2c.h"   /* hi2c1，i2cscan 使用 */
#include <stdio.h>
#include <string.h>

/* ========== 共享静态缓冲区（节约RAM）========== */
static FileEntry_t s_fs_entries[FS_FILE_MAX]; /* cmd_ls / cmd_status 共用 */

/* ========== 命令处理函数 ========== */

static void cmd_help(int argc, char *argv[])
{
    uint8_t count, i;
    const CLICommand_t *table;
    char buf[60];

    (void)argc;
    (void)argv;

    table = CLI_GetCmdTable(&count);

    BSP_UART_SendString("Commands:\r\n");
    for (i = 0; i < count; i++)
    {
        snprintf(buf, sizeof(buf), "  %-12s - %s\r\n", table[i].name, table[i].help);
        BSP_UART_SendString(buf);
    }
}

static void cmd_ls(int argc, char *argv[])
{
    uint8_t count = 0;
    uint8_t i;
    char buf[60];
    const char *type_str[] = {"LOG", "CONFIG", "DATA", "EVENTS"};
    const uint8_t type_count = (uint8_t)(sizeof(type_str) / sizeof(type_str[0]));

    (void)argc;
    (void)argv;

    FS_File_List(s_fs_entries, FS_FILE_MAX, &count);

    BSP_UART_SendString("Name             Size       Type\r\n");
    BSP_UART_SendString("--------------------------------\r\n");

    if (count == 0)
    {
        BSP_UART_SendString("(empty)\r\n");
        return;
    }

    for (i = 0; i < count; i++)
    {
        const char *type = (s_fs_entries[i].type < type_count)
                         ? type_str[s_fs_entries[i].type] : "UNK";
        snprintf(buf, sizeof(buf), "%-16s %-10lu %s\r\n",
                 s_fs_entries[i].name, s_fs_entries[i].size, type);
        BSP_UART_SendString(buf);
    }
}

static void cmd_cat(int argc, char *argv[])
{
    FileEntry_t entry;
    uint8_t    *buf;
    uint32_t    read_len;
    uint32_t    i, j;
    char        line[80];

    if (argc < 2)
    {
        BSP_UART_SendString("Usage: cat <filename>\r\n");
        return;
    }

    if (FS_File_Find(argv[1], &entry, NULL) != FS_OK)
    {
        BSP_UART_SendString("File not found: ");
        BSP_UART_SendString(argv[1]);
        BSP_UART_SendString("\r\n");
        return;
    }

    if (entry.size == 0)
    {
        BSP_UART_SendString("(empty)\r\n");
        return;
    }

    buf = (uint8_t *)pvPortMalloc(entry.size);
    if (buf == NULL)
    {
        BSP_UART_SendString("ERROR: No memory\r\n");
        return;
    }

    if (FS_File_Read(argv[1], buf, entry.size, &read_len) != FS_OK)
    {
        BSP_UART_SendString("ERROR: Read failed\r\n");
        vPortFree(buf);
        return;
    }

    if (entry.type == FILE_TYPE_LOG)
    {
        /* 逐条解析LogRecord_t */
        uint32_t count = read_len / sizeof(LogRecord_t);
        LogRecord_t *rec = (LogRecord_t *)buf;
        for (i = 0; i < count; i++)
        {
            snprintf(line, sizeof(line), "[%lu][%u] %s\r\n",
                     rec[i].timestamp, rec[i].level, rec[i].msg);
            BSP_UART_SendString(line);
        }
    }
    else
    {
        /* 十六进制打印 */
        for (i = 0; i < read_len; i += 16)
        {
            snprintf(line, sizeof(line), "%04lX: ", i);
            BSP_UART_SendString(line);

            for (j = 0; j < 16 && (i + j) < read_len; j++)
            {
                snprintf(line, sizeof(line), "%02X ", buf[i + j]);
                BSP_UART_SendString(line);
            }
            for (; j < 16; j++)
            {
                BSP_UART_SendString("   ");
            }

            BSP_UART_SendString(" ");
            for (j = 0; j < 16 && (i + j) < read_len; j++)
            {
                char ch = (char)buf[i + j];
                char s[2] = {(ch >= 0x20 && ch <= 0x7E) ? ch : '.', '\0'};
                BSP_UART_SendString(s);
            }
            BSP_UART_SendString("\r\n");
        }
    }

    vPortFree(buf);
}

static void cmd_rm(int argc, char *argv[])
{
    FSResult_e res;

    if (argc < 2)
    {
        BSP_UART_SendString("Usage: rm <filename>\r\n");
        return;
    }

    res = FS_File_Delete(argv[1]);
    if (res == FS_OK)
    {
        BSP_UART_SendString("Deleted: ");
        BSP_UART_SendString(argv[1]);
        BSP_UART_SendString("\r\n");
    }
    else
    {
        BSP_UART_SendString("ERROR: File not found or delete failed\r\n");
    }
}

static void cmd_erase(int argc, char *argv[])
{
    uint32_t addr;
    uint32_t count = 0;
    char buf[32];

    (void)argc;
    (void)argv;

    if (!BSP_Flash_Lock(500))
    {
        BSP_UART_SendString("ERROR: Flash busy, try later\r\n");
        return;
    }

    BSP_UART_SendString("Erasing data area...\r\n");

    addr = FS_ADDR_DATA_START;
    while (addr <= FS_ADDR_DATA_END)
    {
        if (((addr & (W25Q_BLOCK_SIZE - 1)) == 0) &&
            ((addr + W25Q_BLOCK_SIZE) <= (FS_ADDR_DATA_END + 1)))
        {
            BSP_Flash_BlockErase(addr);
            addr += W25Q_BLOCK_SIZE;
            count += 16;
        }
        else
        {
            BSP_Flash_SectorErase(addr);
            addr += W25Q_SECTOR_SIZE;
            count++;
        }

        if ((count & 0x3F) == 0) /* 每64扇区一个点 */
        {
            BSP_UART_SendString(".");
        }
    }

    snprintf(buf, sizeof(buf), "\r\nErase complete, %lu sectors.\r\n", count);
    BSP_UART_SendString(buf);

    BSP_Flash_Unlock();
}

static void cmd_status(int argc, char *argv[])
{
    uint32_t used, total;
    uint8_t file_count = 0;
    char buf[48];

    (void)argc;
    (void)argv;

    FS_GetStatus(&used, &total);
    FS_File_List(s_fs_entries, FS_FILE_MAX, &file_count);

    snprintf(buf, sizeof(buf), "Flash used: %lu / %lu bytes\r\n", used, total);
    BSP_UART_SendString(buf);
    snprintf(buf, sizeof(buf), "Files: %u\r\n", file_count);
    BSP_UART_SendString(buf);
    snprintf(buf, sizeof(buf), "LastFault: %s(%lu) task=%s\r\n",
             Fault_CodeName(Fault_GetLastCode()),
             (unsigned long)Fault_GetLastCode(),
             Fault_GetLastTaskName());
    BSP_UART_SendString(buf);
    snprintf(buf, sizeof(buf), "Boot: %lu  RCC_CSR: 0x%08lX\r\n",
             (unsigned long)Fault_GetBootCount(),
             (unsigned long)Fault_GetResetFlags());
    BSP_UART_SendString(buf);
}

static void cmd_task(int argc, char *argv[])
{
    char buf[512];

    (void)argc;
    (void)argv;

    vTaskList(buf);
    BSP_UART_SendString(buf);
}

static void cmd_heap(int argc, char *argv[])
{
    char buf[64];

    (void)argc;
    (void)argv;

    snprintf(buf, sizeof(buf), "Free heap: %u (min ever: %u)\r\n",
             xPortGetFreeHeapSize(),
             xPortGetMinimumEverFreeHeapSize());
    BSP_UART_SendString(buf);
}

static void cmd_set(int argc, char *argv[])
{
    if (argc < 3)
    {
        BSP_UART_SendString("Usage: set <key> <value>\r\n");
        return;
    }
    if (Config_Set(argv[1], argv[2]) == HAL_OK)
    {
        BSP_UART_SendString("OK. Use 'save' to persist.\r\n");
    }
    else
    {
        BSP_UART_SendString("Unknown key\r\n");
    }
}

static void cmd_get(int argc, char *argv[])
{
    char buf[48];
    if (argc < 2)
    {
        BSP_UART_SendString("Usage: get <key>\r\n");
        return;
    }
    if (Config_Get(argv[1], buf, sizeof(buf)) == HAL_OK)
    {
        BSP_UART_SendString(buf);
        BSP_UART_SendString("\r\n");
    }
    else
    {
        BSP_UART_SendString("Unknown key\r\n");
    }
}

static void cmd_save(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    if (Config_Save() == HAL_OK)
    {
        BSP_UART_SendString("Config saved.\r\n");
    }
    else
    {
        BSP_UART_SendString("Save failed!\r\n");
    }
}

static void cmd_loglvl(int argc, char *argv[])
{
    int level;
    char buf[32];

    if (argc < 2)
    {
        BSP_UART_SendString("Usage: loglvl <0-3>\r\n");
        return;
    }

    level = (int)Utils_StrToInt(argv[1]);
    if (level < LOG_LEVEL_DEBUG || level > LOG_LEVEL_ERROR)
    {
        BSP_UART_SendString("Invalid level: 0=DEBUG,1=INFO,2=WARN,3=ERROR\r\n");
        return;
    }

    g_log_level = (LogLevel_e)level;
    snprintf(buf, sizeof(buf), "Log level = %d\r\n", level);
    BSP_UART_SendString(buf);
}

static void cmd_reset(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    BSP_UART_SendString("Resetting...\r\n");
    HAL_Delay(100);
    NVIC_SystemReset();
}

static void cmd_time(int argc, char *argv[])
{
    char buf[64];
    DateTime_t dt;

    if (argc >= 2)
    {
        uint32_t utc = (uint32_t)Utils_StrToInt(argv[1]);
        if (Recorder_SetUtc(utc))
        {
            BSP_UART_SendString("time set\r\n");
        }
        else
        {
            BSP_UART_SendString("time set FAILED (RTC missing?)\r\n");
        }
        return;
    }

    if (Recorder_GetUtc() == 0U || !Time_FromUnix(Recorder_GetUtc(), &dt))
    {
        BSP_UART_SendString("clock: NOT SET\r\n");
        return;
    }

    snprintf(buf, sizeof(buf), "clock: %04u-%02u-%02u %02u:%02u:%02u UTC\r\n",
             (unsigned)dt.year, (unsigned)dt.month, (unsigned)dt.day,
             (unsigned)dt.hour, (unsigned)dt.minute, (unsigned)dt.second);
    BSP_UART_SendString(buf);
}

static void cmd_alarm(int argc, char *argv[])
{
    char buf[80];
    const AlarmCtx_t *ctx = Recorder_GetAlarm();

    (void)argc;
    (void)argv;

    if (ctx == NULL)
    {
        BSP_UART_SendString("alarm: unavailable\r\n");
        return;
    }

    snprintf(buf, sizeof(buf), "limit: %d..%d hyst=%d dur=%us\r\n",
             (int)ctx->cfg.low_x100, (int)ctx->cfg.high_x100,
             (int)ctx->cfg.hyst_x100, (unsigned)ctx->cfg.min_duration_s);
    BSP_UART_SendString(buf);

    snprintf(buf, sizeof(buf), "state: %s  events=%lu\r\n",
             Alarm_IsActive(ctx) ? "ACTIVE" : "normal",
             (unsigned long)Recorder_GetEventCount());
    BSP_UART_SendString(buf);
}

static void cmd_mkt(int argc, char *argv[])
{
    char buf[48];
    int16_t mkt = Recorder_GetMkt();

    (void)argc;
    (void)argv;

    snprintf(buf, sizeof(buf), "MKT: %d.%02d C\r\n",
             (int)(mkt / 100), (int)((mkt < 0 ? -mkt : mkt) % 100));
    BSP_UART_SendString(buf);
}

/**
 * @brief 扫描 I2C1 总线上的器件
 * @note  接线调试时最省时间的工具：接上模块后先跑一次，
 *        确认 0x3C（OLED）和 0x51（PCF8563）都能应答。
 *        只做存在性探测（HAL_I2C_IsDeviceReady），不读写寄存器。
 */
static void cmd_i2cscan(int argc, char *argv[])
{
    uint8_t  addr;
    uint8_t  found = 0;
    char     buf[40];

    (void)argc;
    (void)argv;

    BSP_UART_SendString("Scanning I2C1 (PB6/PB7)...\r\n");

    for (addr = 0x08U; addr < 0x78U; addr++)
    {
        uint16_t dev = (uint16_t)((uint16_t)addr << 1);

        if (HAL_I2C_IsDeviceReady(&hi2c1, dev, 2U, 10U) == HAL_OK)
        {
            const char *name = "";
            if (addr == 0x3CU)      name = " (SSD1306 OLED)";
            else if (addr == 0x51U) name = " (PCF8563 RTC)";

            snprintf(buf, sizeof(buf), "  0x%02X%s\r\n", addr, name);
            BSP_UART_SendString(buf);
            found++;
        }
    }

    if (found == 0U)
    {
        BSP_UART_SendString("  (no devices found)\r\n");
        BSP_UART_SendString("  check: VCC=3.3V, GND common, SCL=PB6, SDA=PB7\r\n");
    }
    else
    {
        snprintf(buf, sizeof(buf), "  %u device(s)\r\n", (unsigned)found);
        BSP_UART_SendString(buf);
    }
}

/* ========== 命令注册 ========== */

void CLI_RegisterAllCmds(void)
{
    CLI_Register("help",   cmd_help,   "Show all commands");
    CLI_Register("ls",     cmd_ls,     "List files");
    CLI_Register("cat",    cmd_cat,    "Read file content");
    CLI_Register("rm",     cmd_rm,     "Delete file");
    CLI_Register("erase",  cmd_erase,  "Erase all data sectors");
    CLI_Register("status", cmd_status, "Show flash usage & file count");
    CLI_Register("task",   cmd_task,   "Show FreeRTOS task list");
    CLI_Register("heap",   cmd_heap,   "Show heap usage");
    CLI_Register("set",    cmd_set,    "Set config <key> <val>");
    CLI_Register("get",    cmd_get,    "Get config <key>");
    CLI_Register("save",   cmd_save,   "Save config to flash");
    CLI_Register("loglvl", cmd_loglvl, "Set log level (0-3)");
    CLI_Register("reset",  cmd_reset,  "System reset");
    CLI_Register("time",   cmd_time,   "Show/set clock: time [utc]");
    CLI_Register("alarm",  cmd_alarm,  "Show alarm limits and state");
    CLI_Register("mkt",    cmd_mkt,    "Show mean kinetic temperature");
    CLI_Register("i2cscan", cmd_i2cscan, "Scan I2C1 bus for devices");
}
