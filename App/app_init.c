/**
 * @file    app_init.c
 * @brief   应用层初始化 - 任务骨架与IPC对象实现
 * @author  SLK800
 * @date    2026-05-01
 * @version 1.0
 */

#include "app_init.h"
#include "task_logger.h"
#include "task_shell.h"
#include "task_data_producer.h"
#include "task_display.h"
#include "task_monitor.h"
#include "log.h"
#include "config.h"
#include "sensor_ntc.h"
#include "ui_status.h"
#include "cli.h"
#include "bsp_uart.h"
#include "bsp_flash.h"
#include "fs_core.h"
#include "fs_file.h"
#include <stdio.h>

/* ========== IPC对象定义 ========== */
static EventGroupHandle_t s_SysEvents = NULL;

/* ========== 任务句柄定义 ========== */
TaskHandle_t g_hTaskLogger    = NULL;
TaskHandle_t g_hTaskShell     = NULL;
TaskHandle_t g_hTaskDataProd  = NULL;
TaskHandle_t g_hTaskDisplay   = NULL;
TaskHandle_t g_hTaskMonitor   = NULL;

volatile uint32_t g_fault_code = 0U;
volatile uint32_t g_fault_task_handle = 0U;
volatile char g_fault_task_name[16];

/* ========== FreeRTOS钩子函数 ========== */

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    uint8_t i;

    g_fault_code = 1U;
    g_fault_task_handle = (uint32_t)xTask;
    for (i = 0U; i < (uint8_t)(sizeof(g_fault_task_name) - 1U); i++)
    {
        if (pcTaskName == NULL || pcTaskName[i] == '\0')
        {
            break;
        }
        g_fault_task_name[i] = pcTaskName[i];
    }
    g_fault_task_name[i] = '\0';

    for (;;)
    {
        /* 停机 */
    }
}

void vApplicationMallocFailedHook(void)
{
    g_fault_code = 2U;
    g_fault_task_handle = 0U;
    g_fault_task_name[0] = '\0';

    for (;;)
    {
        /* 停机 */
    }
}

/* ========== 应用初始化函数 ========== */

void App_Init(void)
{
    /* 1. 初始化串口 */
    BSP_UART_Init();

    /* 2. 初始化Flash */
    if (BSP_Flash_Init() != HAL_OK)
    {
        BSP_UART_SendString("[SYS] Flash Init FAILED!\r\n");
        for (;;)
        {
            /* 停机 */
        }
    }

    /* 3. 初始化文件系统 */
    if (FS_Init() == FS_ERR_NOT_INIT)
    {
        BSP_UART_SendString("[SYS] FS not formatted, formatting...\r\n");
        FS_Format();
        if (FS_Init() != FS_OK)
        {
            BSP_UART_SendString("[SYS] FS Init FAILED after format!\r\n");
            for (;;)
            {
                /* 停机 */
            }
        }
    }

    /* 4. 初始化日志模块 */
    Log_Init();

    /* 5. 创建事件组 */
    s_SysEvents = xEventGroupCreate();
    if (s_SysEvents == NULL)
    {
        BSP_UART_SendString("[SYS] SysEvents create FAILED!\r\n");
        for (;;) { /* 停机 */ }
    }

    /* 6. 初始化配置模块（依赖 BSP_Flash_Lock） */
    Config_Init();
    (void)Sensor_NTC_Init();
    UI_Status_Init();

    /* 7. 通知FS已就绪 */
    Sys_EventSet(EVT_BIT_FS_READY);

    /* 8. 创建任务 */
    if (xTaskCreate(Task_Logger, "Logger", TASK_STACK_LOGGER, NULL,
                    TASK_PRIORITY_LOGGER, &g_hTaskLogger) != pdPASS)
    {
        BSP_UART_SendString("[SYS] Logger task create FAILED!\r\n");
        for (;;)
        {
            /* 停机 */
        }
    }

#if APP_ENABLE_UART_CLI
    if (xTaskCreate(Task_Shell, "Shell", TASK_STACK_SHELL, NULL,
                    TASK_PRIORITY_SHELL, &g_hTaskShell) != pdPASS)
    {
        BSP_UART_SendString("[SYS] Shell task create FAILED!\r\n");
        for (;;)
        {
            /* 停机 */
        }
    }
#endif

    if (xTaskCreate(Task_DataProducer, "DataProd", TASK_STACK_DATA_PROD, NULL,
                    TASK_PRIORITY_DATA_PROD, &g_hTaskDataProd) != pdPASS)
    {
        BSP_UART_SendString("[SYS] DataProd task create FAILED!\r\n");
        for (;;)
        {
            /* 停机 */
        }
    }

    if (xTaskCreate(Task_Display, "Display", TASK_STACK_DISPLAY, NULL,
                    TASK_PRIORITY_DISPLAY, &g_hTaskDisplay) != pdPASS)
    {
        BSP_UART_SendString("[SYS] Display task create FAILED!\r\n");
        for (;;)
        {
            /* 鍋滄満 */
        }
    }

#if APP_ENABLE_UART_MONITOR
    if (xTaskCreate(Task_Monitor, "Monitor", TASK_STACK_MONITOR, NULL,
                    TASK_PRIORITY_MONITOR, &g_hTaskMonitor) != pdPASS)
    {
        BSP_UART_SendString("[SYS] Monitor task create FAILED!\r\n");
        for (;;)
        {
            /* 停机 */
        }
    }
#endif

    /* 9. 打印任务创建完成信息 */
    BSP_UART_SendString("[SYS] All tasks created. Starting scheduler...\r\n");

    /* 10. 启动调度器 */
    vTaskStartScheduler();

    /* 不应执行到这里 */
    BSP_UART_SendString("[SYS] Scheduler start FAILED!\r\n");
    for (;;)
    {
        /* 停机 */
    }
}

/* ========== 系统事件API ========== */

void Sys_EventSet(EventBits_t bits)
{
    if (s_SysEvents != NULL)
    {
        xEventGroupSetBits(s_SysEvents, bits);
    }
}

bool Sys_EventWait(EventBits_t bits, uint32_t timeout_ms)
{
    if (s_SysEvents == NULL)
    {
        return false;
    }
    return (xEventGroupWaitBits(s_SysEvents, bits, pdFALSE, pdTRUE,
                                pdMS_TO_TICKS(timeout_ms)) == bits);
}
