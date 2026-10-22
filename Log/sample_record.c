#include "sample_record.h"
#include "utils.h"
#include <string.h>

void SampleRecord_Fill(SampleRecord_t *record, uint32_t timestamp, uint32_t sample_count, const SensorNtcData_t *ntc)
{
    if (record == NULL || ntc == NULL)
    {
        return;
    }

    memset(record, 0, sizeof(*record));
    record->timestamp = timestamp;
    record->sample_count = sample_count;
    record->temperature_x10 = ntc->temperature_x10;
    record->adc_raw = ntc->adc_raw;
    record->voltage_mv = ntc->voltage_mv;
    record->ntc_status = (uint8_t)ntc->status;
    record->crc = SampleRecord_CalcCRC(record);
}

uint32_t SampleRecord_CalcCRC(const SampleRecord_t *record)
{
    if (record == NULL)
    {
        return 0U;
    }
    return Utils_CRC32_Calc((const uint8_t *)record, sizeof(SampleRecord_t) - sizeof(uint32_t));
}

bool SampleRecord_IsValid(const SampleRecord_t *record)
{
    if (record == NULL)
    {
        return false;
    }
    return record->crc == SampleRecord_CalcCRC(record);
}

bool SampleRecord_IsBlank(const SampleRecord_t *record)
{
    const uint8_t *p;
    uint32_t i;

    if (record == NULL)
    {
        return false;
    }

    p = (const uint8_t *)record;
    for (i = 0U; i < sizeof(SampleRecord_t); i++)
    {
        if (p[i] != 0xFFU)
        {
            return false;
        }
    }
    return true;
}
