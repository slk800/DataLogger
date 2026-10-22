/**
 * @file    domain_record.h
 * @brief   记录格式 v2 - 双通道、墙钟时间、16 字节紧凑记录
 * @author  SLK800
 * @date    2026-05
 * @version 1.0
 *
 * @note
 * v2 记录 16 字节，60 秒采样可存 90 天（129600 条 × 16B ≈ 2.07 MB）。
 *
 *   offset  size  field
 *        0     4  ts_utc     自 1970 的秒数；0 表示时钟未设置
 *        4     2  t1_x100    通道 1 温度 ×100（-40.00..125.00 → -4000..12500）
 *        6     2  t2_x100    通道 2 温度 ×100
 *        8     2  vbat_mv    电池电压
 *       10     1  flags      bit0 通道1故障, bit1 通道2故障, bit2 电池低
 *       11     1  seq        记录序号低 8 位（用于检测丢段）
 *       12     2  aux        预留：报警位 / 扩展字段
 *       14     2  crc16      覆盖前 14 字节
 *
 * 之所以记录级用 CRC16 而不是 CRC32：
 *   - 16 字节记录里 CRC32 占比过高；
 *   - 记录本身很短，CRC16 的漏检率（~1/65536）已远低于 Flash 位翻转概率；
 *   - 文件级仍保留 CRC32，用于导出时端到端比对。
 */

#ifndef __DOMAIN_RECORD_H
#define __DOMAIN_RECORD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define RECORD_V2_SIZE          16U
#define RECORD_TEMP_MIN_X100    (-4000)
#define RECORD_TEMP_MAX_X100    (12500)
#define RECORD_TEMP_INVALID     ((int16_t)(-32768))

/* flags 位定义 */
#define RECORD_FLAG_CH1_FAULT   (1U << 0)
#define RECORD_FLAG_CH2_FAULT   (1U << 1)
#define RECORD_FLAG_VBAT_LOW    (1U << 2)
#define RECORD_FLAG_CLOCK_LOST  (1U << 3)

/* aux 位定义 */
#define RECORD_AUX_ALARM_HIGH   (1U << 0)
#define RECORD_AUX_ALARM_LOW    (1U << 1)
#define RECORD_AUX_ALARM_ACTIVE (1U << 2)

typedef struct {
    uint32_t ts_utc;
    int16_t  t1_x100;
    int16_t  t2_x100;
    uint16_t vbat_mv;
    uint8_t  flags;
    uint8_t  seq;
    uint16_t aux;
} RecordV2_t;

/**
 * @brief 把结构体序列化为 16 字节（小端，含 CRC16）
 */
void Record_Encode(const RecordV2_t *rec, uint8_t out[RECORD_V2_SIZE]);

/**
 * @brief 从 16 字节反序列化并校验 CRC
 * @return true 表示记录有效
 */
bool Record_Decode(const uint8_t in[RECORD_V2_SIZE], RecordV2_t *out);

/**
 * @brief 判断一段字节是否保持擦除态（全 0xFF）
 */
bool Record_IsBlank(const uint8_t in[RECORD_V2_SIZE]);

/**
 * @brief 温度是否在合法量程内
 */
bool Record_TempValid(int16_t t_x100);

#endif /* __DOMAIN_RECORD_H */
