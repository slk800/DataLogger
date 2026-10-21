/*
 * 协议帧层主机侧测试（纯软件，不依赖硬件）。
 *
 * 覆盖：
 *   - CRC16/CCITT-FALSE 已知向量
 *   - COBS 往返、含零字节、254/255 边界
 *   - 组帧 / 解帧往返
 *   - 坏 CRC、坏长度、坏版本
 *   - 流式接收器：拆包、粘包、超长丢弃、定界符重同步
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "proto_frame.h"

static int failures;

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s  (line %d)\n", msg, __LINE__); failures++; } \
    else         { printf("ok  : %s\n", msg); } \
} while (0)

/* ---------- CRC ---------- */
static void test_crc(void)
{
    /* CRC-16/CCITT-FALSE("123456789") = 0x29B1 */
    CHECK(Proto_CRC16((const uint8_t *)"123456789", 9U) == 0x29B1U,
          "CRC16/CCITT-FALSE known vector");
    CHECK(Proto_CRC16(NULL, 0U) == 0xFFFFU, "CRC16 of NULL returns init");
}

/* ---------- COBS ---------- */
static void test_cobs(void)
{
    uint8_t src[512];
    uint8_t enc[600];
    uint8_t dec[600];
    uint32_t i, enc_len, dec_len;

    /* 1. 普通数据往返 */
    const uint8_t a[] = {0x11, 0x22, 0x00, 0x33};
    enc_len = Proto_COBSEncode(a, sizeof(a), enc, sizeof(enc));
    CHECK(enc_len > 0U, "COBS encode with zero");
    CHECK(memchr(enc, 0x00, enc_len) == NULL, "COBS output has no zero byte");
    dec_len = Proto_COBSDecode(enc, enc_len, dec, sizeof(dec));
    CHECK(dec_len == sizeof(a) && memcmp(a, dec, sizeof(a)) == 0,
          "COBS roundtrip with zero");

    /* 2. 全零 */
    memset(src, 0, 16);
    enc_len = Proto_COBSEncode(src, 16U, enc, sizeof(enc));
    dec_len = Proto_COBSDecode(enc, enc_len, dec, sizeof(dec));
    CHECK(dec_len == 16U && memcmp(src, dec, 16U) == 0, "COBS roundtrip all-zero");

    /* 3. 无零字节 */
    for (i = 0; i < 100U; i++) src[i] = (uint8_t)(i + 1U);
    enc_len = Proto_COBSEncode(src, 100U, enc, sizeof(enc));
    dec_len = Proto_COBSDecode(enc, enc_len, dec, sizeof(dec));
    CHECK(dec_len == 100U && memcmp(src, dec, 100U) == 0, "COBS roundtrip no-zero");

    /* 4. 254 / 255 边界（触发 code==0xFF 分支） */
    for (i = 0; i < 254U; i++) src[i] = (uint8_t)(i + 1U);
    enc_len = Proto_COBSEncode(src, 254U, enc, sizeof(enc));
    dec_len = Proto_COBSDecode(enc, enc_len, dec, sizeof(dec));
    CHECK(dec_len == 254U && memcmp(src, dec, 254U) == 0, "COBS roundtrip 254 bytes");

    for (i = 0; i < 255U; i++) src[i] = (uint8_t)(i + 1U);
    enc_len = Proto_COBSEncode(src, 255U, enc, sizeof(enc));
    dec_len = Proto_COBSDecode(enc, enc_len, dec, sizeof(dec));
    CHECK(dec_len == 255U && memcmp(src, dec, 255U) == 0, "COBS roundtrip 255 bytes");

    /* 5. 容量不足 */
    CHECK(Proto_COBSEncode(src, 255U, enc, 3U) == 0U, "COBS rejects small buffer");
}

/* ---------- 组帧 / 解帧 ---------- */
static void test_frame(void)
{
    uint8_t buf[PROTO_MAX_FRAME * 2];
    uint8_t payload[32];
    ProtoFrame_t f;
    uint32_t n;
    int32_t rc;
    uint32_t i;

    for (i = 0; i < sizeof(payload); i++) payload[i] = (uint8_t)(i * 7U);

    n = Proto_BuildFrame(PROTO_T_READ, 0x1234U, payload, sizeof(payload),
                         buf, sizeof(buf));
    CHECK(n > 0U, "build frame");
    CHECK(buf[n - 1U] == 0x00U, "frame ends with delimiter");
    CHECK(memchr(buf, 0x00, n - 1U) == NULL, "frame body has no zero");

    rc = Proto_ParseFrame(buf, n - 1U, &f);
    CHECK(rc == 0, "parse frame");
    CHECK(f.ver == PROTO_VER, "parsed version");
    CHECK(f.type == PROTO_T_READ, "parsed type");
    CHECK(f.seq == 0x1234U, "parsed seq (little endian)");
    CHECK(f.len == sizeof(payload), "parsed len");
    CHECK(memcmp(f.data, payload, sizeof(payload)) == 0, "parsed payload");

    /* 空载荷 */
    n = Proto_BuildFrame(PROTO_T_STAT, 1U, NULL, 0U, buf, sizeof(buf));
    rc = Proto_ParseFrame(buf, n - 1U, &f);
    CHECK(rc == 0 && f.len == 0U && f.type == PROTO_T_STAT, "empty payload frame");

    /* 最大载荷 */
    for (i = 0; i < PROTO_MAX_DATA; i++) payload[i % sizeof(payload)] = (uint8_t)i;
    n = Proto_BuildFrame(PROTO_T_READ, 7U, payload, PROTO_MAX_DATA,
                         buf, sizeof(buf));
    CHECK(n > 0U, "build max payload frame");
    rc = Proto_ParseFrame(buf, n - 1U, &f);
    CHECK(rc == 0 && f.len == PROTO_MAX_DATA, "parse max payload frame");

    /* 超长拒绝 */
    CHECK(Proto_BuildFrame(PROTO_T_READ, 1U, payload,
                           PROTO_MAX_DATA + 1U, buf, sizeof(buf)) == 0U,
          "reject oversize payload");
}

/* ---------- 错误路径 ---------- */
static void test_errors(void)
{
    uint8_t buf[PROTO_MAX_FRAME];
    ProtoFrame_t f;
    uint32_t n;
    int32_t rc;

    /* 坏 CRC：翻转最后一个数据字节 */
    n = Proto_BuildFrame(PROTO_T_HELLO, 1U, NULL, 0U, buf, sizeof(buf));
    buf[n - 2U] ^= 0xFFU;       /* 破坏 CRC 高字节 */
    CHECK(Proto_ParseFrame(buf, n - 1U, &f) == -2, "detect bad CRC");

    /* 坏版本 */
    n = Proto_BuildFrame(PROTO_T_HELLO, 1U, NULL, 0U, buf, sizeof(buf));
    {
        uint8_t raw[PROTO_MAX_FRAME];
        uint32_t raw_len = Proto_COBSDecode(buf, n - 1U, raw, sizeof(raw));
        uint8_t enc[PROTO_MAX_FRAME];
        uint32_t enc_len;
        raw[0] = 0x99U;         /* 改版本，重新编码（CRC 必然不匹配） */
        enc_len = Proto_COBSEncode(raw, raw_len, enc, sizeof(enc));
        rc = Proto_ParseFrame(enc, enc_len, &f);
        CHECK(rc == -3, "detect unsupported version");
    }

    /* 截断帧 */
    n = Proto_BuildFrame(PROTO_T_HELLO, 1U, NULL, 0U, buf, sizeof(buf));
    CHECK(Proto_ParseFrame(buf, n / 2U, &f) != 0, "reject truncated frame");

    /* 空输入 */
    CHECK(Proto_ParseFrame(buf, 0U, &f) == -1, "reject empty input");
}

/* ---------- 流式接收 ---------- */
static void test_stream(void)
{
    uint8_t f1[PROTO_MAX_FRAME];
    uint8_t f2[PROTO_MAX_FRAME];
    uint32_t n1, n2;
    ProtoRx_t rx;
    ProtoFrame_t out;
    int32_t got = 0;
    uint32_t i;
    uint8_t payload[64];

    for (i = 0; i < sizeof(payload); i++) payload[i] = (uint8_t)(i + 1U);

    n1 = Proto_BuildFrame(PROTO_T_LS, 11U, NULL, 0U, f1, sizeof(f1));
    n2 = Proto_BuildFrame(PROTO_T_READ, 22U, payload, sizeof(payload), f2, sizeof(f2));

    /* 1. 粘包：两帧连续喂入 */
    Proto_RxReset(&rx);
    for (i = 0; i < n1; i++)
    {
        if (Proto_RxFeed(&rx, f1[i], &out) == 1)
        {
            CHECK(out.seq == 11U && out.type == PROTO_T_LS, "stream: first frame");
            got++;
        }
    }
    for (i = 0; i < n2; i++)
    {
        if (Proto_RxFeed(&rx, f2[i], &out) == 1)
        {
            CHECK(out.seq == 22U && out.len == sizeof(payload), "stream: second frame");
            got++;
        }
    }
    CHECK(got == 2, "stream: both frames parsed");

    /* 2. 垃圾字节前缀：应能在下一个定界符重新同步 */
    Proto_RxReset(&rx);
    for (i = 0; i < 5U; i++) Proto_RxFeed(&rx, 0xAAU, NULL);
    Proto_RxFeed(&rx, 0x00U, NULL);     /* 垃圾被当成一帧丢弃 */
    got = 0;
    for (i = 0; i < n1; i++)
    {
        if (Proto_RxFeed(&rx, f1[i], &out) == 1) got++;
    }
    CHECK(got == 1, "stream: resync after garbage");

    /* 3. 超长帧：丢弃后仍能接收下一帧 */
    Proto_RxReset(&rx);
    for (i = 0; i < 400U; i++) Proto_RxFeed(&rx, 0x55U, NULL);
    CHECK(rx.drop_count == 1U, "stream: oversize frame dropped");
    Proto_RxFeed(&rx, 0x00U, NULL);
    got = 0;
    for (i = 0; i < n1; i++)
    {
        if (Proto_RxFeed(&rx, f1[i], &out) == 1) got++;
    }
    CHECK(got == 1, "stream: works after oversize drop");
}

int main(void)
{
    test_crc();
    test_cobs();
    test_frame();
    test_errors();
    test_stream();

    printf("\nproto_test: %s (%d failures)\n", failures ? "FAILED" : "PASSED", failures);
    return failures ? 1 : 0;
}
