/* Host-side compile check stub for FreeRTOS semphr.h */
#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include "FreeRTOS.h"

SemaphoreHandle_t xSemaphoreCreateMutex(void);
BaseType_t        xSemaphoreTake(SemaphoreHandle_t s, TickType_t wait);
BaseType_t        xSemaphoreGive(SemaphoreHandle_t s);

#endif /* SEMAPHORE_H */
