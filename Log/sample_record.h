#ifndef __SAMPLE_RECORD_H
#define __SAMPLE_RECORD_H

#include <stdint.h>
#include <stdbool.h>
#include "sensor_ntc.h"

typedef struct {
    uint32_t timestamp;
    uint32_t sample_count;
    int32_t temperature_x10;
    uint16_t adc_raw;
    uint16_t voltage_mv;
    uint8_t ntc_status;
    uint8_t reserved[3];
    uint32_t crc;
} SampleRecord_t;

void SampleRecord_Fill(SampleRecord_t *record, uint32_t timestamp, uint32_t sample_count, const SensorNtcData_t *ntc);
uint32_t SampleRecord_CalcCRC(const SampleRecord_t *record);
bool SampleRecord_IsValid(const SampleRecord_t *record);
bool SampleRecord_IsBlank(const SampleRecord_t *record);

#endif /* __SAMPLE_RECORD_H */
