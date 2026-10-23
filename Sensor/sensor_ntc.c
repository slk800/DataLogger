/**
 * @file    sensor_ntc.c
 * @brief   NTC 采样 - 双通道过采样、Beta 换算与状态分类
 * @author  SLK800
 * @date    2026-05
 * @version 1.1
 *
 * @note
 * 硬件：ADC1 扫描模式，Rank1=IN0(PA0)，Rank2=IN1(PA1)。
 *       每次 HAL_ADC_Start 会依次转换两个通道，各产生一次 EOC。
 *
 * 处理链（每通道独立）：
 *   16 次过采样 → 去极值平均 → 电压/电阻换算 → B 参数方程 → 一阶 IIR
 *
 * 滤波状态按通道分开保存。如果两路共用一个滤波器，交替采样会让
 * 两个温度互相"污染"，读数会出现莫名其妙的滞后。
 */

#include "sensor_ntc.h"
#include "adc.h"
#include <math.h>
#include <string.h>

#define SENSOR_NTC_SAMPLE_COUNT 16U

/* 每通道独立的滤波状态 */
static uint8_t s_filter_valid[SENSOR_NTC_CH_COUNT];
static int32_t s_filtered_temp_x10[SENSOR_NTC_CH_COUNT];

HAL_StatusTypeDef Sensor_NTC_Init(void)
{
    memset(s_filter_valid, 0, sizeof(s_filter_valid));
    memset(s_filtered_temp_x10, 0, sizeof(s_filtered_temp_x10));
    return HAL_ADCEx_Calibration_Start(&hadc1);
}

/**
 * @brief 一次扫描读出两个通道的原始值
 * @return HAL_OK 表示两路都读到了
 * @note  F1 扫描模式下每个通道转换完成都会置 EOC，
 *        所以这里 Poll 两次，第一次拿 Rank1，第二次拿 Rank2。
 */
static HAL_StatusTypeDef Sensor_NTC_ReadPair(uint16_t *raw0, uint16_t *raw1)
{
    HAL_StatusTypeDef ret;

    if (raw0 == NULL || raw1 == NULL)
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
    *raw0 = (uint16_t)HAL_ADC_GetValue(&hadc1);

    ret = HAL_ADC_PollForConversion(&hadc1, 10U);
    if (ret != HAL_OK)
    {
        (void)HAL_ADC_Stop(&hadc1);
        return ret;
    }
    *raw1 = (uint16_t)HAL_ADC_GetValue(&hadc1);

    (void)HAL_ADC_Stop(&hadc1);
    return HAL_OK;
}

/**
 * @brief 双通道过采样平均（去最大最小值）
 * @param ch 0 或 1
 */
static HAL_StatusTypeDef Sensor_NTC_ReadAverage(uint8_t ch, uint16_t *raw)
{
    uint32_t sum = 0U;
    uint16_t min = 0xFFFFU;
    uint16_t max = 0U;
    uint8_t i;

    if (raw == NULL || ch >= SENSOR_NTC_CH_COUNT)
    {
        return HAL_ERROR;
    }

    for (i = 0U; i < SENSOR_NTC_SAMPLE_COUNT; i++)
    {
        uint16_t r0 = 0U;
        uint16_t r1 = 0U;
        uint16_t sample;

        if (Sensor_NTC_ReadPair(&r0, &r1) != HAL_OK)
        {
            return HAL_ERROR;
        }

        sample = (ch == 0U) ? r0 : r1;
        if (sample > SENSOR_NTC_ADC_MAX)
        {
            sample = SENSOR_NTC_ADC_MAX;
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

static int32_t Sensor_NTC_Filter(uint8_t ch, int32_t temp_x10)
{
    if (ch >= SENSOR_NTC_CH_COUNT)
    {
        return temp_x10;
    }

    if (s_filter_valid[ch] == 0U)
    {
        s_filtered_temp_x10[ch] = temp_x10;
        s_filter_valid[ch] = 1U;
    }
    else
    {
        s_filtered_temp_x10[ch] = ((s_filtered_temp_x10[ch] * 7) + temp_x10) / 8;
    }
    return s_filtered_temp_x10[ch];
}

/**
 * @brief 把一个原始 ADC 值转换为完整读数
 */
static void Sensor_NTC_Convert(uint8_t ch, uint16_t raw, SensorNtcData_t *out)
{
    int32_t temp_x10;

    memset(out, 0, sizeof(*out));

    out->adc_raw = raw;
    out->voltage_mv = Sensor_NTC_ToVoltageMv(raw);

    if (raw < SENSOR_NTC_SHORT_ADC)
    {
        out->status = SENSOR_NTC_ERR_SHORT;
        return;
    }
    if (raw > SENSOR_NTC_OPEN_ADC)
    {
        out->status = SENSOR_NTC_ERR_OPEN;
        return;
    }

    out->resistance_ohm = Sensor_NTC_ToResistanceOhm(out->voltage_mv);
    temp_x10 = Sensor_NTC_ToTempX10(out->resistance_ohm);

    if (temp_x10 < SENSOR_NTC_MIN_TEMP_X10 || temp_x10 > SENSOR_NTC_MAX_TEMP_X10)
    {
        out->temperature_x10 = temp_x10;
        out->status = SENSOR_NTC_ERR_RANGE;
        return;
    }

    out->temperature_x10 = Sensor_NTC_Filter(ch, temp_x10);
    out->status = SENSOR_NTC_OK;
}

HAL_StatusTypeDef Sensor_NTC_ReadChannel(uint8_t ch, SensorNtcData_t *out)
{
    uint16_t raw;

    if (out == NULL || ch >= SENSOR_NTC_CH_COUNT)
    {
        return HAL_ERROR;
    }

    if (Sensor_NTC_ReadAverage(ch, &raw) != HAL_OK)
    {
        memset(out, 0, sizeof(*out));
        out->status = SENSOR_NTC_ERR_ADC;
        return HAL_ERROR;
    }

    Sensor_NTC_Convert(ch, raw, out);
    return HAL_OK;
}

HAL_StatusTypeDef Sensor_NTC_Read(SensorNtcData_t *out)
{
    return Sensor_NTC_ReadChannel(0U, out);
}

HAL_StatusTypeDef Sensor_NTC_ReadBoth(SensorNtcData_t *ch0, SensorNtcData_t *ch1)
{
    uint16_t raw[SENSOR_NTC_CH_COUNT];
    uint32_t sum[SENSOR_NTC_CH_COUNT] = {0U, 0U};
    uint16_t min[SENSOR_NTC_CH_COUNT] = {0xFFFFU, 0xFFFFU};
    uint16_t max[SENSOR_NTC_CH_COUNT] = {0U, 0U};
    uint8_t i;
    uint8_t ch;

    if (ch0 == NULL && ch1 == NULL)
    {
        return HAL_ERROR;
    }

    for (i = 0U; i < SENSOR_NTC_SAMPLE_COUNT; i++)
    {
        uint16_t r[SENSOR_NTC_CH_COUNT];

        if (Sensor_NTC_ReadPair(&r[0], &r[1]) != HAL_OK)
        {
            if (ch0 != NULL) { memset(ch0, 0, sizeof(*ch0)); ch0->status = SENSOR_NTC_ERR_ADC; }
            if (ch1 != NULL) { memset(ch1, 0, sizeof(*ch1)); ch1->status = SENSOR_NTC_ERR_ADC; }
            return HAL_ERROR;
        }

        for (ch = 0U; ch < SENSOR_NTC_CH_COUNT; ch++)
        {
            uint16_t v = r[ch];
            if (v > SENSOR_NTC_ADC_MAX)
            {
                v = SENSOR_NTC_ADC_MAX;
            }
            sum[ch] += v;
            if (v < min[ch]) min[ch] = v;
            if (v > max[ch]) max[ch] = v;
        }
    }

    for (ch = 0U; ch < SENSOR_NTC_CH_COUNT; ch++)
    {
        raw[ch] = (uint16_t)((sum[ch] - min[ch] - max[ch]) / (SENSOR_NTC_SAMPLE_COUNT - 2U));
    }

    if (ch0 != NULL)
    {
        Sensor_NTC_Convert(0U, raw[0], ch0);
    }
    if (ch1 != NULL)
    {
        Sensor_NTC_Convert(1U, raw[1], ch1);
    }

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
