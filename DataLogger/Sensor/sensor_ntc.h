#ifndef __SENSOR_NTC_H
#define __SENSOR_NTC_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

#define SENSOR_NTC_ADC_MAX          4095U
#define SENSOR_NTC_VREF_MV          3300U
#define SENSOR_NTC_R_FIXED_OHM      10000UL
#define SENSOR_NTC_R0_OHM           10000UL
#define SENSOR_NTC_B_VALUE          3950.0f
#define SENSOR_NTC_T0_K             298.15f
#define SENSOR_NTC_SHORT_ADC        20U
#define SENSOR_NTC_OPEN_ADC         4075U
#define SENSOR_NTC_MIN_TEMP_X10     (-200)
#define SENSOR_NTC_MAX_TEMP_X10     1250

typedef enum {
    SENSOR_NTC_OK = 0,
    SENSOR_NTC_ERR_SHORT,
    SENSOR_NTC_ERR_OPEN,
    SENSOR_NTC_ERR_RANGE,
    SENSOR_NTC_ERR_ADC
} SensorNtcStatus_e;

typedef struct {
    uint16_t adc_raw;
    uint16_t voltage_mv;
    uint32_t resistance_ohm;
    int32_t temperature_x10;
    SensorNtcStatus_e status;
} SensorNtcData_t;

HAL_StatusTypeDef Sensor_NTC_Init(void);
HAL_StatusTypeDef Sensor_NTC_Read(SensorNtcData_t *out);
const char *Sensor_NTC_StatusName(SensorNtcStatus_e status);

#endif /* __SENSOR_NTC_H */
