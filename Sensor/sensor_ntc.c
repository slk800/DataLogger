#include "sensor_ntc.h"
#include "adc.h"
#include <math.h>
#include <string.h>

#define SENSOR_NTC_SAMPLE_COUNT 16U

static uint8_t s_filter_valid = 0U;
static int32_t s_filtered_temp_x10 = 0;

HAL_StatusTypeDef Sensor_NTC_Init(void)
{
    s_filter_valid = 0U;
    s_filtered_temp_x10 = 0;
    return HAL_ADCEx_Calibration_Start(&hadc1);
}

static HAL_StatusTypeDef Sensor_NTC_ReadOne(uint16_t *raw)
{
    HAL_StatusTypeDef ret;
    uint32_t value;

    if (raw == NULL)
    {
        return HAL_ERROR;
    }

    ret = HAL_ADC_Start(&hadc1);
    if (ret != HAL_OK)
    {
        return ret;
    }

    ret = HAL_ADC_PollForConversion(&hadc1, 10U);
    if (ret != HAL_OK)
    {
        (void)HAL_ADC_Stop(&hadc1);
        return ret;
    }

    value = HAL_ADC_GetValue(&hadc1);
    (void)HAL_ADC_Stop(&hadc1);

    if (value > SENSOR_NTC_ADC_MAX)
    {
        value = SENSOR_NTC_ADC_MAX;
    }

    *raw = (uint16_t)value;
    return HAL_OK;
}

static HAL_StatusTypeDef Sensor_NTC_ReadAverage(uint16_t *raw)
{
    uint32_t sum = 0U;
    uint16_t min = 0xFFFFU;
    uint16_t max = 0U;
    uint8_t i;

    if (raw == NULL)
    {
        return HAL_ERROR;
    }

    for (i = 0U; i < SENSOR_NTC_SAMPLE_COUNT; i++)
    {
        uint16_t sample;
        if (Sensor_NTC_ReadOne(&sample) != HAL_OK)
        {
            return HAL_ERROR;
        }
        sum += sample;
        if (sample < min)
        {
            min = sample;
        }
        if (sample > max)
        {
            max = sample;
        }
    }

    sum -= min;
    sum -= max;
    *raw = (uint16_t)(sum / (SENSOR_NTC_SAMPLE_COUNT - 2U));
    return HAL_OK;
}

static uint16_t Sensor_NTC_ToVoltageMv(uint16_t raw)
{
    return (uint16_t)(((uint32_t)raw * SENSOR_NTC_VREF_MV) / SENSOR_NTC_ADC_MAX);
}

static uint32_t Sensor_NTC_ToResistanceOhm(uint16_t voltage_mv)
{
    uint32_t denominator;

    if (voltage_mv == 0U)
    {
        return 0U;
    }
    if (voltage_mv >= SENSOR_NTC_VREF_MV)
    {
        return 0xFFFFFFFFUL;
    }

    denominator = SENSOR_NTC_VREF_MV - voltage_mv;
    return (SENSOR_NTC_R_FIXED_OHM * (uint32_t)voltage_mv) / denominator;
}

static int32_t Sensor_NTC_ToTempX10(uint32_t resistance_ohm)
{
    float r;
    float inv_t;
    float temp_k;
    float temp_c;

    if (resistance_ohm == 0U || resistance_ohm == 0xFFFFFFFFUL)
    {
        return 0;
    }

    r = (float)resistance_ohm;
    inv_t = (1.0f / SENSOR_NTC_T0_K) + (logf(r / (float)SENSOR_NTC_R0_OHM) / SENSOR_NTC_B_VALUE);
    temp_k = 1.0f / inv_t;
    temp_c = temp_k - 273.15f;
    return (int32_t)(temp_c * 10.0f);
}

static int32_t Sensor_NTC_Filter(int32_t temp_x10)
{
    if (s_filter_valid == 0U)
    {
        s_filtered_temp_x10 = temp_x10;
        s_filter_valid = 1U;
    }
    else
    {
        s_filtered_temp_x10 = ((s_filtered_temp_x10 * 7) + temp_x10) / 8;
    }
    return s_filtered_temp_x10;
}

HAL_StatusTypeDef Sensor_NTC_Read(SensorNtcData_t *out)
{
    uint16_t raw;
    int32_t temp_x10;

    if (out == NULL)
    {
        return HAL_ERROR;
    }

    memset(out, 0, sizeof(*out));

    if (Sensor_NTC_ReadAverage(&raw) != HAL_OK)
    {
        out->status = SENSOR_NTC_ERR_ADC;
        return HAL_ERROR;
    }

    out->adc_raw = raw;
    out->voltage_mv = Sensor_NTC_ToVoltageMv(raw);

    if (raw < SENSOR_NTC_SHORT_ADC)
    {
        out->status = SENSOR_NTC_ERR_SHORT;
        return HAL_OK;
    }
    if (raw > SENSOR_NTC_OPEN_ADC)
    {
        out->status = SENSOR_NTC_ERR_OPEN;
        return HAL_OK;
    }

    out->resistance_ohm = Sensor_NTC_ToResistanceOhm(out->voltage_mv);
    temp_x10 = Sensor_NTC_ToTempX10(out->resistance_ohm);

    if (temp_x10 < SENSOR_NTC_MIN_TEMP_X10 || temp_x10 > SENSOR_NTC_MAX_TEMP_X10)
    {
        out->temperature_x10 = temp_x10;
        out->status = SENSOR_NTC_ERR_RANGE;
        return HAL_OK;
    }

    out->temperature_x10 = Sensor_NTC_Filter(temp_x10);
    out->status = SENSOR_NTC_OK;
    return HAL_OK;
}

const char *Sensor_NTC_StatusName(SensorNtcStatus_e status)
{
    switch (status)
    {
    case SENSOR_NTC_OK:
        return "OK";
    case SENSOR_NTC_ERR_SHORT:
        return "SHORT";
    case SENSOR_NTC_ERR_OPEN:
        return "OPEN";
    case SENSOR_NTC_ERR_RANGE:
        return "RANGE";
    case SENSOR_NTC_ERR_ADC:
        return "ADC";
    default:
        return "ERR";
    }
}
