/**
 * @file    utils.h
 * @brief   通用工具函数头文件 - CRC32计算、字符串处理等
 * @author  [Your Name]
 * @date    2025-01-01
 * @version 1.0
 */

#ifndef __UTILS_H
#define __UTILS_H

#include "global.h"

/* ========== 宏定义 ========== */
#define UTILS_OK    0       /* 操作成功 */
#define UTILS_ERR   (-1)    /* 操作失败 */

/* ========== 函数声明 ========== */

/**
 * @brief 计算CRC32校验值
 * @param data 数据指针
 * @param len  数据长度
 * @return CRC32校验值
 * @note  使用IEEE 802.3标准多项式 0xEDB88320，查表法实现
 */
uint32_t Utils_CRC32_Calc(const uint8_t *data, uint32_t len);

/**
 * @brief 字符串转整数
 * @param str 待转换的字符串
 * @return 转换后的整数值
 * @note  支持十进制，支持负数（前缀'-'），不依赖atoi
 */
int32_t Utils_StrToInt(const char *str);

/**
 * @brief 整数转字符串
 * @param val    待转换的整数值
 * @param buf    输出缓冲区
 * @param bufLen 缓冲区长度
 */
void Utils_IntToStr(int32_t val, char *buf, uint8_t bufLen);

/**
 * @brief 去除字符串首尾空格
 * @param str 待处理的字符串（原地修改）
 */
void Utils_StrTrim(char *str);

/**
 * @brief 字符串分割
 * @param str        待分割的字符串（会被修改）
 * @param delim      分隔符
 * @param tokens     token指针数组
 * @param maxTokens  最大token数量
 * @return 实际分割出的token数量
 * @note  该函数会修改原字符串（替换delim为'\0'）
 */
uint8_t Utils_StrSplit(char *str, char delim, char *tokens[], uint8_t maxTokens);

/**
 * @brief CRC32校验验证
 * @param data     数据指针
 * @param len      数据长度
 * @param expected 期望的CRC32值
 * @return 1: 校验一致, 0: 校验不一致
 */
uint8_t Utils_CRC32_Verify(const uint8_t *data, uint32_t len, uint32_t expected);

#endif /* __UTILS_H */
