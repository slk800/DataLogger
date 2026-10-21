/**
 * @file    proto_frame.h
 * @brief   串口协议帧层 - COBS 编解码、CRC16 与帧解析
 * @author  SLK800
 * @date    2026-05
 * @version 1.0
 *
 * 本模块是纯逻辑，不依赖 HAL、FreeRTOS 或 Flash：
 *   - 设备端：由 Task_Proto 调用
 *   - 主机端：tests/proto_test.c 直接链接同一份源码做单测
 */

#ifndef __PROTO_FRAME_H
#define __PROTO_FRAME_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ========== 协议常量 ========== */
#define PROTO_VER               0x01U
#define PROTO_DELIMITER         0x00U
#define PROTO_HEADER_SIZE       6U      /* ver + type + seq(2) + len(2) */
#define PROTO_MAX_DATA          256U    /* 单帧 data 上限 */
#define PROTO_MAX_FRAME         (PROTO_MAX_DATA + PROTO_HEADER_SIZE + 2U)

/*
 * COBS 最坏情况：每 254 字节增加 1 字节。
 * 编码缓冲区需要 ceil(len / 254) 字节的额外空间。
 */
#define PROTO_COBS_MAX(src_len) ((src_len) + ((src_len) / 254U) + 1U)

/* ========== 帧类型 ========== */
typedef enum {
    PROTO_T_HELLO     = 0x01,
    PROTO_T_LS        = 0x02,
    PROTO_T_STAT      = 0x03,
    PROTO_T_READ      = 0x04,
    PROTO_T_CRC       = 0x05,
    PROTO_T_EVENTS    = 0x06,
    PROTO_T_GET_CFG   = 0x07,
    PROTO_T_SET_CFG   = 0x08,
    PROTO_T_SAVE_CFG  = 0x09,
    PROTO_T_ERASE     = 0x0A,
    PROTO_T_SET_TIME  = 0x0B,
    PROTO_T_GET_TIME  = 0x0C,
    PROTO_T_ACK       = 0x80,
    PROTO_T_NAK       = 0x81
} ProtoType_e;

/* ========== 错误码 ========== */
typedef enum {
    PROTO_ERR_OK           = 0x00,
    PROTO_ERR_BAD_CRC      = 0x01,
    PROTO_ERR_BAD_LEN      = 0x02,
    PROTO_ERR_NO_FILE      = 0x03,
    PROTO_ERR_NO_SPACE     = 0x04,
    PROTO_ERR_FLASH        = 0x05,
    PROTO_ERR_UNSUPPORTED  = 0x06,
    PROTO_ERR_BUSY         = 0x07,
    PROTO_ERR_DENIED       = 0x08
} ProtoErr_e;

/* ========== 解析出的帧 ========== */
typedef struct {
    uint8_t  ver;
    uint8_t  type;
    uint16_t seq;
    uint16_t len;
    uint8_t  data[PROTO_MAX_DATA];
} ProtoFrame_t;

/* ========== CRC / COBS 基础函数 ========== */

/**
 * @brief CRC-16/CCITT-FALSE
 * @param data 数据指针
 * @param len  字节数
 * @return 16 位校验值
 */
uint16_t Proto_CRC16(const uint8_t *data, uint32_t len);

/**
 * @brief COBS 编码
 * @param src      源数据
 * @param src_len  源长度
 * @param dst      目标缓冲区
 * @param dst_cap  目标容量
 * @return 编码后长度；0 表示失败（容量不足或参数非法）
 */
uint32_t Proto_COBSEncode(const uint8_t *src, uint32_t src_len,
                          uint8_t *dst, uint32_t dst_cap);

/**
 * @brief COBS 解码
 * @param src      编码数据（不含结尾 0x00）
 * @param src_len  编码长度
 * @param dst      目标缓冲区
 * @param dst_cap  目标容量
 * @return 解码后长度；0 表示失败
 */
uint32_t Proto_COBSDecode(const uint8_t *src, uint32_t src_len,
                          uint8_t *dst, uint32_t dst_cap);

/* ========== 组帧 ========== */

/**
 * @brief 构造完整帧（含 COBS 编码与 0x00 定界符）
 * @param type     帧类型
 * @param seq      序号
 * @param data     载荷，可为 NULL
 * @param data_len 载荷长度
 * @param out      输出缓冲区
 * @param out_cap  输出容量
 * @return 写入 out 的字节数（含定界符）；0 表示失败
 */
uint32_t Proto_BuildFrame(uint8_t type, uint16_t seq,
                          const uint8_t *data, uint16_t data_len,
                          uint8_t *out, uint32_t out_cap);

/**
 * @brief 构造 ACK 帧
 */
uint32_t Proto_BuildAck(uint16_t seq, const uint8_t *data, uint16_t data_len,
                        uint8_t *out, uint32_t out_cap);

/**
 * @brief 构造 NAK 帧
 */
uint32_t Proto_BuildNak(uint16_t seq, uint8_t err_code,
                        uint8_t *out, uint32_t out_cap);

/* ========== 解帧 ========== */

/**
 * @brief 解析一个已去掉定界符的 COBS 帧
 * @param raw      收到的一帧（不含 0x00）
 * @param raw_len  长度
 * @param out      解析结果
 * @return 0 成功；负值表示失败（-1 帧格式错误，-2 CRC 错误，-3 版本不支持）
 */
int32_t Proto_ParseFrame(const uint8_t *raw, uint32_t raw_len, ProtoFrame_t *out);

/* ========== 流式接收器 ========== */

typedef struct {
    uint8_t  buf[PROTO_MAX_FRAME];
    uint32_t len;
    uint32_t drop_count;    /* 超长帧被丢弃的次数 */
} ProtoRx_t;

/**
 * @brief 复位接收器
 */
void Proto_RxReset(ProtoRx_t *rx);

/**
 * @brief 向接收器喂一个字节
 * @param rx   接收器
 * @param byte 收到的字节
 * @param out  当凑齐完整帧时写入解析结果（可为 NULL，此时只做同步）
 * @return 1 = 收到完整且合法的帧；0 = 继续接收；-1 = 收到帧但解析失败
 */
int32_t Proto_RxFeed(ProtoRx_t *rx, uint8_t byte, ProtoFrame_t *out);

#endif /* __PROTO_FRAME_H */
