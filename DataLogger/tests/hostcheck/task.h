/* Host-side compile check stub for FreeRTOS task.h */

#ifndef INC_TASK_H
#define INC_TASK_H

#include "FreeRTOS.h"

typedef void (*TaskFunction_t)(void *);

BaseType_t xTaskCreate(TaskFunction_t fn, const char *name,
                       uint16_t stackDepth, void *param,
                       UBaseType_t priority, TaskHandle_t *handle);
void       vTaskDelay(TickType_t ticks);
void       vTaskDelayUntil(TickType_t *prev, TickType_t inc);
void       vTaskStartScheduler(void);
void       vTaskDelete(TaskHandle_t task);
TickType_t xTaskGetTickCount(void);
void       vTaskStepTick(TickType_t ticks);
void       vTaskSuspendAll(void);
BaseType_t xTaskResumeAll(void);
void       vTaskList(char *buf);
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t task);

#endif /* INC_TASK_H */
