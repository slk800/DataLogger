/**
 * @file    app_init.h
 * @brief   应用层初始化 - 任务定义与IPC对象声明
 * @author  SLK800
 * @date    2026-05-01
 * @version 1.0
 */

#ifndef __APP_INIT_H
#define __APP_INIT_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"
#include <stdbool.h>

/* ========== 任务优先级定义 ========== */
#define TASK_PRIORITY_MONITOR      1
#define TASK_PRIORITY_DATA_PROD    2
#define TASK_PRIORITY_DISPLAY      2
#define TASK_PRIORITY_LOGGER       3
#define TASK_PRIORITY_SHELL        4

/* ========== 任务栈大小定义 (单位: words) ========== */
#define TASK_STACK_MONITOR         128
#define TASK_STACK_DATA_PROD       256
#define TASK_STACK_DISPLAY         256
#define TASK_STACK_LOGGER          256
#define TASK_STACK_SHELL           256

#ifndef APP_ENABLE_UART_CLI
#define APP_ENABLE_UART_CLI        0
#endif

#ifndef APP_ENABLE_UART_MONITOR
#define APP_ENABLE_UART_MONITOR    0
#endif

/* ========== 事件组位定义 ========== */
#define EVT_BIT_FS_READY           ((EventBits_t)(1 << 0))
#define EVT_BIT_LOG_FULL           ((EventBits_t)(1 << 1))
#define EVT_BIT_CMD_ERASE          ((EventBits_t)(1 << 2))
#define EVT_BIT_CFG_CHANGED        ((EventBits_t)(1 << 3))

/* ========== 任务句柄声明 ========== */
extern TaskHandle_t g_hTaskLogger;
extern TaskHandle_t g_hTaskShell;
extern TaskHandle_t g_hTaskDataProd;
extern TaskHandle_t g_hTaskDisplay;
extern TaskHandle_t g_hTaskMonitor;

/* ========== 函数声明 ========== */

void App_Init(void);

void Sys_EventSet(EventBits_t bits);
bool Sys_EventWait(EventBits_t bits, uint32_t timeout_ms);

#endif /* __APP_INIT_H */
