/**
 * @file    ui_status.c
 * @brief   Mutex-protected OLED UI status snapshot.
 */

#include "global.h"

static UiStatus_t s_ui_status;
static SemaphoreHandle_t s_ui_mutex;
static uint8_t s_sensor_updated;

static void UI_Status_CopyConfig(UiStatus_t *status)
{
    status->sample_ms = g_config.sample_interval_ms;
    status->log_level = g_config.log_level;
    strncpy(status->device_name, g_config.device_name, sizeof(status->device_name) - 1U);
    status->device_name[sizeof(status->device_name) - 1U] = '\0';
}

void UI_Status_Init(void)
{
    memset(&s_ui_status, 0, sizeof(s_ui_status));
    s_sensor_updated = 0U;
    s_ui_status.ntc_status = SENSOR_NTC_ERR_ADC;
    s_ui_mutex = xSemaphoreCreateMutex();
    if (s_ui_mutex != NULL)
    {
        xSemaphoreTake(s_ui_mutex, portMAX_DELAY);
        UI_Status_CopyConfig(&s_ui_status);
        s_ui_status.flash_ok = 1U;
        s_ui_status.fs_ok = 1U;
        xSemaphoreGive(s_ui_mutex);
    }
}

void UI_Status_SetOledOk(uint8_t ok)
{
    if (s_ui_mutex == NULL)
    {
        return;
    }
    xSemaphoreTake(s_ui_mutex, portMAX_DELAY);
    s_ui_status.oled_ok = ok ? 1U : 0U;
    xSemaphoreGive(s_ui_mutex);
}

void UI_Status_UpdateProducer(uint32_t sample_count, int32_t temp_x10, const char *msg)
{
    if (s_ui_mutex == NULL)
    {
        return;
    }
    xSemaphoreTake(s_ui_mutex, portMAX_DELAY);
    s_ui_status.sample_count = sample_count;
    if (s_sensor_updated == 0U)
    {
        s_ui_status.latest_temp_x10 = temp_x10;
        s_ui_status.ntc_status = SENSOR_NTC_OK;
    }
    if (msg != NULL)
    {
        strncpy(s_ui_status.latest_msg, msg, sizeof(s_ui_status.latest_msg) - 1U);
        s_ui_status.latest_msg[sizeof(s_ui_status.latest_msg) - 1U] = '\0';
    }
    xSemaphoreGive(s_ui_mutex);
}

void UI_Status_UpdateSensor(const SensorNtcData_t *data)
{
    if (data == NULL)
    {
        return;
    }
    if (s_ui_mutex == NULL)
    {
        return;
    }
    xSemaphoreTake(s_ui_mutex, portMAX_DELAY);
    s_ui_status.latest_temp_x10 = data->temperature_x10;
    s_ui_status.adc_raw = data->adc_raw;
    s_ui_status.voltage_mv = data->voltage_mv;
    s_ui_status.resistance_ohm = data->resistance_ohm;
    s_ui_status.ntc_status = (uint8_t)data->status;
    s_sensor_updated = 1U;
    xSemaphoreGive(s_ui_mutex);
}

void UI_Status_UpdateSystem(void)
{
    if (s_ui_mutex == NULL)
    {
        return;
    }
    xSemaphoreTake(s_ui_mutex, portMAX_DELAY);
    s_ui_status.tick_ms = xTaskGetTickCount();
    s_ui_status.run_seconds = s_ui_status.tick_ms / 1000U;
    s_ui_status.log_dropped = g_log_dropped;
    s_ui_status.heap_free = xPortGetFreeHeapSize();
    s_ui_status.heap_min = xPortGetMinimumEverFreeHeapSize();
    UI_Status_CopyConfig(&s_ui_status);
    xSemaphoreGive(s_ui_mutex);
}

void UI_Status_UpdateFlash(uint32_t used, uint32_t total, uint8_t file_count)
{
    if (s_ui_mutex == NULL)
    {
        return;
    }
    xSemaphoreTake(s_ui_mutex, portMAX_DELAY);
    s_ui_status.flash_used = used;
    s_ui_status.flash_total = total;
    s_ui_status.file_count = file_count;
    xSemaphoreGive(s_ui_mutex);
}

void UI_Status_GetSnapshot(UiStatus_t *out)
{
    if (out == NULL)
    {
        return;
    }
    if (s_ui_mutex == NULL)
    {
        memset(out, 0, sizeof(*out));
        return;
    }
    xSemaphoreTake(s_ui_mutex, portMAX_DELAY);
    memcpy(out, &s_ui_status, sizeof(*out));
    xSemaphoreGive(s_ui_mutex);
}
