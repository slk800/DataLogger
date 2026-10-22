/* Host-side compile check stub for FreeRTOS event_groups.h */
#ifndef EVENT_GROUPS_H
#define EVENT_GROUPS_H

#include "FreeRTOS.h"

EventGroupHandle_t xEventGroupCreate(void);
EventBits_t        xEventGroupSetBits(EventGroupHandle_t g, EventBits_t bits);
EventBits_t        xEventGroupWaitBits(EventGroupHandle_t g, EventBits_t bits,
                                       BaseType_t clearOnExit,
                                       BaseType_t waitAll, TickType_t wait);

#endif /* EVENT_GROUPS_H */
