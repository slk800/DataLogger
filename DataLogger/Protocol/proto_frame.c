/**
 * @file    proto_frame.c
 * @brief   串口协议帧层实现 - COBS 编解码、CRC16 与帧解析
 * @author  SLK800
 * @date    2026-05
 * @version 1.0
 */

#include "proto_frame.h"

/* ========== CRC-16/CCITT-FALSE ========== */

uint16_t Proto_CRC16(const uint8_t *data, uint32_t len)
{
    uint16_t crc = 0xFFFFU;
    uint32_t i;
    uint8_t  bit;

    if (data == NULL && len > 0U)
    {
        return 0U;      /* 参数非法；空输入合法，返回 init 值 */
    }

    for (i = 0U; i < len; i++)
    {
        crc ^= (uint16_t)((uint16_t)data[i] << 8);
        for (bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 0x8000U) != 0U)
            {
                crc = (uint16_t)((uint16_t)(crc << 1) ^ 0x1021U);
            }
            else
            {
                crc = (uint16_t)(crc << 1);
            }
        }
    }

    return crc;
}

/* ========== COBS ========== */

uint32_t Proto_COBSEncode(const uint8_t *src, uint32_t src_len,
                          uint8_t *dst, uint32_t dst_cap)
{
    uint32_t read_idx  = 0U;
    uint32_t write_idx = 1U;    /* 先留出第一个 code 字节 */
    uint32_t code_idx  = 0U;
    uint8_t  code      = 1U;

    if (dst == NULL || (src == NULL && src_len > 0U))
    {
        return 0U;
    }
    if (dst_cap < 2U)
    {
        return 0U;
    }

    while (read_idx < src_len)
    {
        if (src[read_idx] == 0x00U)
        {
            /* 遇到零：写入当前 code，跳过零字节 */
            if (code_idx >= dst_cap)
            {
                return 0U;
            }
            dst[code_idx] = code;
            code_idx = write_idx;
            write_idx++;
            code = 1U;
            read_idx++;
        }
        else
        {
            if (write_idx >= dst_cap)
            {
                return 0U;
            }
            dst[write_idx++] = src[read_idx++];
            code++;

            if (code == 0xFFU)
            {
                /* 连续 254 个非零字节：强制结束当前块 */
                if (code_idx >= dst_cap)
                {
                    return 0U;
                }
                dst[code_idx] = code;
                code_idx = write_idx;
                write_idx++;
                code = 1U;
            }
        }
    }

    /* 收尾：写最后一个 code */
    if (code_idx < dst_cap)
    {
        dst[code_idx] = code;
    }
    else
    {
        /* 需要写 code 但没有空间 */
        if (write_idx > 0U)
        {
            write_idx--;
        }
        return 0U;
    }

    return write_idx;
}

uint32_t Proto_COBSDecode(const uint8_t *src, uint32_t src_len,
                          uint8_t *dst, uint32_t dst_cap)
{
    uint32_t read_idx  = 0U;
    uint32_t write_idx = 0U;

    if (src == NULL || dst == NULL || src_len == 0U)
    {
        return 0U;
    }

    while (read_idx < src_len)
    {
        uint8_t code = src[read_idx++];
        uint32_t i;

        if (code == 0U)
        {
            return 0U;      /* 帧内不应出现 0x00 */
        }

        for (i = 1U; i < (uint32_t)code; i++)
        {
            if (read_idx >= src_len)
            {
                return 0U;  /* 数据被截断 */
            }
            if (write_idx >= dst_cap)
            {
                return 0U;
            }
            dst[write_idx++] = src[read_idx++];
        }

        /*
         * code 之后的隐式零字节：
         *   - 最后一个块不补；
         *   - code == 0xFF 的满块也不补（该边界是长度截断，不是零字节）。
         */
        if (read_idx < src_len && code != 0xFFU)
        {
            if (write_idx >= dst_cap)
            {
                return 0U;
            }
            dst[write_idx++] = 0x00U;
        }
    }

    return write_idx;
}

/* ========== 组帧 ========== */

uint32_t Proto_BuildFrame(uint8_t type, uint16_t seq,
                          const uint8_t *data, uint16_t data_len,
                          uint8_t *out, uint32_t out_cap)
{
    uint8_t  raw[PROTO_MAX_FRAME];
    uint32_t raw_len;
    uint32_t enc_len;
    uint16_t crc;
    uint32_t pos = 0U;

    if (out == NULL || (data == NULL && data_len > 0U))
    {
        return 0U;
    }
    if (data_len > PROTO_MAX_DATA)
    {
        return 0U;
    }

    /* 1. 头部 */
    raw[pos++] = PROTO_VER;
    raw[pos++] = type;
    raw[pos++] = (uint8_t)(seq & 0xFFU);
    raw[pos++] = (uint8_t)((seq >> 8) & 0xFFU);
    raw[pos++] = (uint8_t)(data_len & 0xFFU);
    raw[pos++] = (uint8_t)((data_len >> 8) & 0xFFU);

    /* 2. 载荷 */
    if (data_len > 0U)
    {
        uint16_t i;
        for (i = 0U; i < data_len; i++)
        {
            raw[pos++] = data[i];
        }
    }

    raw_len = pos;

    /* 3. CRC16 覆盖 ver..data */
    crc = Proto_CRC16(raw, raw_len);
    raw[pos++] = (uint8_t)(crc & 0xFFU);
    raw[pos++] = (uint8_t)((crc >> 8) & 0xFFU);
    raw_len = pos;

    /* 4. COBS 编码 + 定界符 */
    if (out_cap < 2U)
    {
        return 0U;
    }

    enc_len = Proto_COBSEncode(raw, raw_len, out, out_cap - 1U);
    if (enc_len == 0U || enc_len >= out_cap)
    {
        return 0U;
    }

    out[enc_len] = PROTO_DELIMITER;

    return enc_len + 1U;
}

uint32_t Proto_BuildAck(uint16_t seq, const uint8_t *data, uint16_t data_len,
                        uint8_t *out, uint32_t out_cap)
{
    return Proto_BuildFrame(PROTO_T_ACK, seq, data, data_len, out, out_cap);
}

uint32_t Proto_BuildNak(uint16_t seq, uint8_t err_code,
                        uint8_t *out, uint32_t out_cap)
{
    return Proto_BuildFrame(PROTO_T_NAK, seq, &err_code, 1U, out, out_cap);
}

/* ========== 解帧 ========== */

int32_t Proto_ParseFrame(const uint8_t *raw, uint32_t raw_len, ProtoFrame_t *out)
{
    uint8_t  decoded[PROTO_MAX_FRAME];
    uint32_t dec_len;
    uint16_t data_len;
    uint16_t crc_calc;
    uint16_t crc_recv;

    if (raw == NULL || out == NULL || raw_len < (PROTO_HEADER_SIZE + 2U))
    {
        return -1;
    }

    /* 1. COBS 解码 */
    dec_len = Proto_COBSDecode(raw, raw_len, decoded, sizeof(decoded));
    if (dec_len < (PROTO_HEADER_SIZE + 2U))
    {
        return -1;
    }

    /* 2. 头部校验 */
    if (decoded[0] != PROTO_VER)
    {
        return -3;
    }

    data_len = (uint16_t)((uint16_t)decoded[4] | ((uint16_t)decoded[5] << 8));
    if (data_len > PROTO_MAX_DATA)
    {
        return -1;
    }
    if (dec_len != (PROTO_HEADER_SIZE + (uint32_t)data_len + 2U))
    {
        return -1;
    }

    /* 3. CRC */
    crc_calc = Proto_CRC16(decoded, PROTO_HEADER_SIZE + data_len);
    crc_recv = (uint16_t)((uint16_t)decoded[dec_len - 2U] |
                          ((uint16_t)decoded[dec_len - 1U] << 8));
    if (crc_calc != crc_recv)
    {
        return -2;
    }

    /* 4. 输出 */
    out->ver  = decoded[0];
    out->type = decoded[1];
    out->seq  = (uint16_t)((uint16_t)decoded[2] | ((uint16_t)decoded[3] << 8));
    out->len  = data_len;
    if (data_len > 0U)
    {
        uint16_t i;
        for (i = 0U; i < data_len; i++)
        {
            out->data[i] = decoded[PROTO_HEADER_SIZE + i];
        }
    }

    return 0;
}

/* ========== 流式接收器 ========== */

void Proto_RxReset(ProtoRx_t *rx)
{
    if (rx == NULL)
    {
        return;
    }
    rx->len = 0U;
    rx->drop_count = 0U;
}

int32_t Proto_RxFeed(ProtoRx_t *rx, uint8_t byte, ProtoFrame_t *out)
{
    int32_t rc;

    if (rx == NULL)
    {
        return 0;
    }

    if (byte != PROTO_DELIMITER)
    {
        if (rx->len < sizeof(rx->buf))
        {
            rx->buf[rx->len++] = byte;
        }
        else
        {
            /* 超长：丢弃整帧，等待下一个定界符重新同步 */
            rx->drop_count++;
            rx->len = 0U;
        }
        return 0;
    }

    /* 收到定界符 */
    if (rx->len == 0U)
    {
        return 0;   /* 连续定界符，忽略 */
    }

    if (out == NULL)
    {
        rx->len = 0U;
        return 0;
    }

    rc = Proto_ParseFrame(rx->buf, rx->len, out);
    rx->len = 0U;

    return (rc == 0) ? 1 : -1;
}
