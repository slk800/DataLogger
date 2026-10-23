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

/* 通道编号：0 = PA0 (IN0)，1 = PA1 (IN1) */
#define SENSOR_NTC_CH_COUNT     2U

HAL_StatusTypeDef Sensor_NTC_Init(void);

/**
 * @brief 读取指定通道
 * @param ch  通道号（0 或 1）
 * @param out 结果
 * @note  每路有独立的 IIR 滤波状态，互不干扰。
 */
HAL_StatusTypeDef Sensor_NTC_ReadChannel(uint8_t ch, SensorNtcData_t *out);

/**
 * @brief 读取通道 0（兼容旧接口）
 */
HAL_StatusTypeDef Sensor_NTC_Read(SensorNtcData_t *out);

/**
 * @brief 一次读双通道（扫描模式下单次转换，比两次调用省一半时间）
 * @param ch0 通道 0 结果，可为 NULL
 * @param ch1 通道 1 结果，可为 NULL
 */
HAL_StatusTypeDef Sensor_NTC_ReadBoth(SensorNtcData_t *ch0, SensorNtcData_t *ch1);

const char *Sensor_NTC_StatusName(SensorNtcStatus_e status);

#endif /* __SENSOR_NTC_H */
