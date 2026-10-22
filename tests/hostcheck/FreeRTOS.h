/*
 * Host-side compile check stub for FreeRTOS.
 *
 * 仅提供本工程使用的类型与 API，用于 PC 端语法/类型检查。
 */

#ifndef INC_FREERTOS_H
#define INC_FREERTOS_H

#include <stdint.h>
#include <stddef.h>

typedef uint32_t TickType_t;
typedef long     BaseType_t;
typedef unsigned long UBaseType_t;
typedef uint32_t EventBits_t;

#define pdTRUE   ((BaseType_t)1)
#define pdFALSE  ((BaseType_t)0)
#define pdPASS   pdTRUE
#define portMAX_DELAY ((TickType_t)0xFFFFFFFFUL)

#define pdMS_TO_TICKS(ms)   ((TickType_t)(ms))

typedef struct tskTaskControlBlock * TaskHandle_t;
typedef struct QueueDefinition     * QueueHandle_t;
typedef struct QueueDefinition     * SemaphoreHandle_t;
typedef struct EventGroupDef_t     * EventGroupHandle_t;

extern uint32_t uxTopUsedPriority;

#ifndef configTOTAL_HEAP_SIZE
#define configTOTAL_HEAP_SIZE ((size_t)7168)
#endif

/* heap 相关 */
size_t xPortGetFreeHeapSize(void);
size_t xPortGetMinimumEverFreeHeapSize(void);
void  *pvPortMalloc(size_t n);
void   vPortFree(void *p);

#endif /* INC_FREERTOS_H */
