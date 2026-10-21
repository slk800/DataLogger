/* Host-side FreeRTOS stubs (compile check only) */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"

static TickType_t s_tick;

BaseType_t xTaskCreate(TaskFunction_t fn, const char *name, uint16_t depth,
                       void *p, UBaseType_t prio, TaskHandle_t *h)
{ (void)fn;(void)name;(void)depth;(void)p;(void)prio; if (h) *h = (TaskHandle_t)1; return pdPASS; }

void vTaskDelay(TickType_t t) { s_tick += t; }
void vTaskDelayUntil(TickType_t *prev, TickType_t inc) { (void)prev; s_tick += inc; }
void vTaskStartScheduler(void) { }
void vTaskDelete(TaskHandle_t t) { (void)t; }
TickType_t xTaskGetTickCount(void) { return s_tick; }
void vTaskStepTick(TickType_t t) { s_tick += t; }
void vTaskSuspendAll(void) { }
BaseType_t xTaskResumeAll(void) { return pdTRUE; }
void vTaskList(char *buf) { if (buf) buf[0] = '\0'; }
UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t t) { (void)t; return 100; }

QueueHandle_t xQueueCreate(UBaseType_t l, UBaseType_t s) { (void)l;(void)s; return (QueueHandle_t)1; }
BaseType_t xQueueSend(QueueHandle_t q, const void *i, TickType_t w) { (void)q;(void)i;(void)w; return pdTRUE; }
BaseType_t xQueueReceive(QueueHandle_t q, void *i, TickType_t w) { (void)q;(void)i;(void)w; return pdFALSE; }

SemaphoreHandle_t xSemaphoreCreateMutex(void) { return (SemaphoreHandle_t)1; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t w) { (void)s;(void)w; return pdTRUE; }
BaseType_t xSemaphoreGive(SemaphoreHandle_t s) { (void)s; return pdTRUE; }

EventGroupHandle_t xEventGroupCreate(void) { return (EventGroupHandle_t)1; }
EventBits_t xEventGroupSetBits(EventGroupHandle_t g, EventBits_t b) { (void)g; return b; }
EventBits_t xEventGroupWaitBits(EventGroupHandle_t g, EventBits_t b, BaseType_t c,
                                BaseType_t a, TickType_t w)
{ (void)g;(void)c;(void)a;(void)w; return b; }

size_t xPortGetFreeHeapSize(void) { return 3000U; }
size_t xPortGetMinimumEverFreeHeapSize(void) { return 2500U; }
void *pvPortMalloc(size_t n) { return malloc(n); }
void vPortFree(void *p) { free(p); }
