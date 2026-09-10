/**
 * @file    task_logger.c
 * @brief   Logger任务 — 从队列取日志记录写入Flash
 * @author  SLK800
 * @date    2026-05-01
 * @version 1.0
 */

#include "global.h"
#include "task_logger.h"

void Task_Logger(void *pvParam)
{
    LogRecord_t record;

    (void)pvParam;

    /* 等待FS就绪（事件在App_Init中已设置，立即返回） */
    Sys_EventWait(EVT_BIT_FS_READY, 10000);

    BSP_UART_SendString("[Logger] started\r\n");

    for (;;)
    {
        if (Log_Dequeue(&record, 2000))
        {
            if (BSP_Flash_Lock(1000))
            {
                (void)FS_File_Append(LOG_TEXT_FILE_NAME, (uint8_t *)&record, sizeof(LogRecord_t));
                BSP_Flash_Unlock();
            }
        }
        else
        {
            /* idle — 不打印，静默等待 */
        }
    }
}
