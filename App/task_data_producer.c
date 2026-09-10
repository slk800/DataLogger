/**
 * @file    task_data_producer.c
 * @brief   DataProducer任务 — 定时产生日志记录并送入队列
 * @author  SLK800
 * @date    2026-05-01
 * @version 1.0
 */

#include "global.h"
#include "task_data_producer.h"

void Task_DataProducer(void *pvParam)
{
    static uint32_t s_counter    = 0;
    static uint32_t s_drop_count = 0;
    static uint32_t s_drop_last  = 0;
    uint32_t tick;
    uint32_t new_drops;

    (void)pvParam;

    BSP_UART_SendString("[DataProd] started\r\n");

    if (BSP_Flash_Lock(1000U))
    {
        s_counter = Log_CountValidSamples();
        BSP_Flash_Unlock();
    }

    for (;;)
    {
        LogRecord_t rec;
        SampleRecord_t sample;
        bool sample_saved = false;

        if (Log_TakeSampleCounterReloadRequest())
        {
            if (BSP_Flash_Lock(1000U))
            {
                s_counter = Log_CountValidSamples();
                BSP_Flash_Unlock();
            }
        }

        tick      = xTaskGetTickCount();
        new_drops = g_log_dropped - s_drop_count;
        s_drop_count = g_log_dropped;

        /* 产生数据，仅入队不刷串口 */
        SensorNtcData_t ntc;
        HAL_StatusTypeDef ntc_ret;

        ntc_ret = Sensor_NTC_Read(&ntc);
        SampleRecord_Fill(&sample, tick, s_counter, &ntc);
        if (BSP_Flash_Lock(1000U))
        {
            if (Log_TakeSampleCounterReloadRequest())
            {
                s_counter = Log_CountValidSamples();
                SampleRecord_Fill(&sample, tick, s_counter, &ntc);
            }
            sample_saved = Log_AppendSample(&sample);
            BSP_Flash_Unlock();
        }
        UI_Status_UpdateSensor(&ntc);

        if (ntc_ret == HAL_OK && ntc.status == SENSOR_NTC_OK)
        {
            int32_t temp_x10 = ntc.temperature_x10;
            const char *temp_sign = temp_x10 < 0 ? "-" : "";
            int32_t temp_abs = temp_x10 < 0 ? -temp_x10 : temp_x10;
            int written;

            rec.level = LOG_LEVEL_INFO;
            written = snprintf(rec.msg, sizeof(rec.msg),
                      "cnt=%lu temp=%s%ld.%ld adc=%u mv=%u tick=%lu",
                      s_counter,
                      temp_sign,
                      (long)(temp_abs / 10),
                      (long)(temp_abs % 10),
                      ntc.adc_raw,
                      ntc.voltage_mv,
                      tick);
            if (written < 0)
            {
                written = 0;
            }
            rec.len = (uint8_t)written;
            if ((size_t)written >= sizeof(rec.msg))
            {
                rec.len = (uint8_t)(sizeof(rec.msg) - 1U);
            }
        }
        else
        {
            int written;

            rec.level = LOG_LEVEL_WARN;
            written = snprintf(rec.msg, sizeof(rec.msg),
                      "cnt=%lu ntc=%s adc=%u mv=%u tick=%lu",
                      s_counter,
                      Sensor_NTC_StatusName(ntc.status),
                      ntc.adc_raw,
                      ntc.voltage_mv,
                      tick);
            if (written < 0)
            {
                written = 0;
            }
            rec.len = (uint8_t)written;
            if ((size_t)written >= sizeof(rec.msg))
            {
                rec.len = (uint8_t)(sizeof(rec.msg) - 1U);
            }
        }

        rec.timestamp = tick;
        rec.crc = Utils_CRC32_Calc((uint8_t *)rec.msg, rec.len);
        if (sample_saved)
        {
            s_counter++;
        }
        UI_Status_UpdateProducer(s_counter, ntc.temperature_x10, rec.msg);
        Log_Submit(&rec);

        if (new_drops > 0)
        {
            s_drop_last = new_drops;
        }

        /* 每100条输出一次丢包统计 */
        if (s_counter > 0U && (s_counter % 100U) == 0U)
        {
            uint32_t total = g_log_dropped;
            LOG_W("Stats: %lu logs, %lu dropped (last burst %lu)",
                  s_counter, total, s_drop_last);
            s_drop_last = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(g_config.sample_interval_ms));
    }
}
