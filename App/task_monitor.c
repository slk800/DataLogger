/**
 * @file    task_monitor.c
 * @brief   Monitor任务 — 周期性输出系统运行状态
 * @author  SLK800
 * @date    2026-05-01
 * @version 1.0
 */

#include "global.h"
#include "task_monitor.h"

void Task_Monitor(void *pvParam)
{
    static char task_buf[400]; /* vTaskList 输出缓冲，静态避免栈溢出 */
    char        buf[80];
    uint32_t    flash_used, flash_total;

    (void)pvParam;

    for (;;)
    {
        FS_GetStatus(&flash_used, &flash_total);
        vTaskList(task_buf);

        BSP_UART_SendString("=== System Monitor ===\r\n");

        snprintf(buf, sizeof(buf), "Tick     : %lu ms\r\n", xTaskGetTickCount());
        BSP_UART_SendString(buf);

        snprintf(buf, sizeof(buf), "Heap     : %u / %u bytes free\r\n",
                 xPortGetFreeHeapSize(), configTOTAL_HEAP_SIZE);
        BSP_UART_SendString(buf);

        snprintf(buf, sizeof(buf), "MinHeap  : %u bytes (historical min)\r\n",
                 xPortGetMinimumEverFreeHeapSize());
        BSP_UART_SendString(buf);

        snprintf(buf, sizeof(buf), "Flash    : %lu / %lu KB used\r\n",
                 flash_used >> 10, flash_total >> 10);
        BSP_UART_SendString(buf);

        snprintf(buf, sizeof(buf), "Dropped  : %lu log records\r\n", g_log_dropped);
        BSP_UART_SendString(buf);

        BSP_UART_SendString("Tasks    : ");
        BSP_UART_SendString(task_buf);

        BSP_UART_SendString("=====================\r\n");

        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}
