/* Host-side compile check stub for FreeRTOS queue.h */
#ifndef QUEUE_H
#define QUEUE_H

#include "FreeRTOS.h"

QueueHandle_t xQueueCreate(UBaseType_t len, UBaseType_t itemSize);
BaseType_t    xQueueSend(QueueHandle_t q, const void *item, TickType_t wait);
BaseType_t    xQueueReceive(QueueHandle_t q, void *item, TickType_t wait);

#endif /* QUEUE_H */
