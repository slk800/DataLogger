/*
 * 打印一组固定帧的十六进制，供 Python 侧交叉验证编解码一致性。
 * 输出格式：  <name> <hex>
 */
#include <stdio.h>
#include <string.h>
#include "proto_frame.h"

static void dump(const char *name, const uint8_t *buf, uint32_t len)
{
    uint32_t i;
    printf("%s ", name);
    for (i = 0; i < len; i++) printf("%02x", buf[i]);
    printf("\n");
}

int main(void)
{
    uint8_t out[PROTO_MAX_FRAME * 2];
    uint8_t payload[PROTO_MAX_DATA];
    uint32_t n;
    uint32_t i;

    /* 1. 空载荷 */
    n = Proto_BuildFrame(PROTO_T_HELLO, 0x0001U, NULL, 0U, out, sizeof(out));
    dump("hello", out, n);

    /* 2. 含 0x00 的载荷 */
    memset(payload, 0, 8);
    payload[3] = 0xAA;
    payload[7] = 0x55;
    n = Proto_BuildFrame(PROTO_T_READ, 0x1234U, payload, 8U, out, sizeof(out));
    dump("read_zeros", out, n);

    /* 3. 最大载荷（触发 COBS 0xFF 边界） */
    for (i = 0; i < PROTO_MAX_DATA; i++) payload[i] = (uint8_t)((i + 1U) & 0xFFU);
    n = Proto_BuildFrame(PROTO_T_READ, 0xBEEFU, payload, PROTO_MAX_DATA, out, sizeof(out));
    dump("read_max", out, n);

    /* 4. NAK */
    n = Proto_BuildNak(0x00FFU, PROTO_ERR_NO_FILE, out, sizeof(out));
    dump("nak", out, n);

    return 0;
}
