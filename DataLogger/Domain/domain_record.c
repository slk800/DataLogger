/**
 * @file    domain_record.c
 * @brief   记录格式 v2 编解码
 * @author  SLK800
 * @date    2026-05
 * @version 1.0
 *
 * @note
 * 不依赖 HAL 与 Utils，自带一份 CRC16/CCITT-FALSE，
 * 这样主机侧单测可以直接链接本文件。
 */

#include "domain_record.h"

/* ========== CRC16/CCITT-FALSE ========== */
static uint16_t Record_CRC16(const uint8_t *data, uint32_t len)
{
    uint16_t crc = 0xFFFFU;
    uint32_t i;
    uint8_t  bit;

    for (i = 0U; i < len; i++)
    {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for (bit = 0U; bit < 8U; bit++)
        {
            crc = ((crc & 0x8000U) != 0U)
                ? (uint16_t)((uint16_t)(crc << 1) ^ 0x1021U)
                : (uint16_t)(crc << 1);
        }
    }

    return crc;
}

/* ========== 小端读写 ========== */
static void PutU16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
}

static void PutU32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
    p[2] = (uint8_t)((v >> 16) & 0xFFU);
    p[3] = (uint8_t)((v >> 24) & 0xFFU);
}

static uint16_t GetU16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t GetU32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

/* ========== 公共接口 ========== */

void Record_Encode(const RecordV2_t *rec, uint8_t out[RECORD_V2_SIZE])
{
    if (rec == NULL || out == NULL)
    {
        return;
    }

    PutU32(&out[0], rec->ts_utc);
    PutU16(&out[4], (uint16_t)rec->t1_x100);
    PutU16(&out[6], (uint16_t)rec->t2_x100);
    PutU16(&out[8], rec->vbat_mv);
    out[10] = rec->flags;
    out[11] = rec->seq;
    PutU16(&out[12], rec->aux);
    PutU16(&out[14], Record_CRC16(out, 14U));
}

bool Record_Decode(const uint8_t in[RECORD_V2_SIZE], RecordV2_t *out)
{
    uint16_t crc;

    if (in == NULL || out == NULL)
    {
        return false;
    }

    crc = GetU16(&in[14]);
    if (crc != Record_CRC16(in, 14U))
    {
        return false;
    }

    out->ts_utc  = GetU32(&in[0]);
    out->t1_x100 = (int16_t)GetU16(&in[4]);
    out->t2_x100 = (int16_t)GetU16(&in[6]);
    out->vbat_mv = GetU16(&in[8]);
    out->flags   = in[10];
    out->seq     = in[11];
    out->aux     = GetU16(&in[12]);

    return true;
}

bool Record_IsBlank(const uint8_t in[RECORD_V2_SIZE])
{
    uint32_t i;

    if (in == NULL)
    {
        return false;
    }
    for (i = 0U; i < RECORD_V2_SIZE; i++)
    {
        if (in[i] != 0xFFU)
        {
            return false;
        }
    }
    return true;
}

bool Record_TempValid(int16_t t_x100)
{
    return (t_x100 >= RECORD_TEMP_MIN_X100) && (t_x100 <= RECORD_TEMP_MAX_X100);
}
