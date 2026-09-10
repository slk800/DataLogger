/**
 * @file    config.h
 * @brief   系统配置模块头文件 - 结构体定义、地址宏与接口声明
 * @author  SLK800
 * @date    2026-05-01
 * @version 1.0
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#include "global.h"

/* ========== Flash地址宏 ========== */
#define CONFIG_ADDR_A   0x004000
#define CONFIG_ADDR_B   0x006000

/* ========== 魔数 ========== */
#define CONFIG_MAGIC    0xCAFEBABE

/* ========== 系统配置结构体 ========== */
typedef struct {
    uint32_t  magic;                    /* 魔数 0xCAFEBABE */
    uint32_t  version;                  /* 配置版本号 */
    uint32_t  sample_interval_ms;       /* 采样间隔 (ms)，默认1000 */
    uint32_t  log_level;                /* 日志等级，默认 LOG_LEVEL_INFO(1) */
    uint32_t  max_log_records;          /* 最大日志条数，默认500 */
    char      device_name[16];          /* 设备名称，默认 "DataLogger" */
    uint8_t   reserved[8];              /* 保留字段 */
    uint32_t  crc;                      /* CRC32 (不含自身) */
} SysConfig_t;

/* ========== 全局配置实例 ========== */
extern SysConfig_t g_config;

/* ========== 函数声明 ========== */

/**
 * @brief 配置模块初始化
 * @note  从Flash加载配置，失败则使用默认值并持久化
 */
void Config_Init(void);

/**
 * @brief 加载默认配置到 g_config (不写Flash)
 */
void Config_LoadDefault(void);

/**
 * @brief 从Flash加载配置
 * @return HAL_OK: 加载成功, HAL_ERROR: 使用了默认值
 * @note  先读A区，失败则读B区并修复A区，均失败则使用默认值
 */
HAL_StatusTypeDef Config_Load(void);

/**
 * @brief 保存配置到Flash (双区交替)
 * @return HAL_OK: 保存成功, HAL_TIMEOUT: 获取锁超时, HAL_ERROR: 验证失败
 * @note  先写B区→验证→再写A区，确保断电安全
 */
HAL_StatusTypeDef Config_Save(void);

/**
 * @brief 设置配置项
 * @param key 配置键名 ("sample_ms"/"log_level"/"max_log"/"name")
 * @param val 配置值字符串
 * @return HAL_OK: 成功, HAL_ERROR: 未知key或值无效
 */
HAL_StatusTypeDef Config_Set(const char *key, const char *val);

/**
 * @brief 获取配置项
 * @param key    配置键名
 * @param buf    输出缓冲区
 * @param bufLen 缓冲区长度
 * @return HAL_OK: 成功, HAL_ERROR: 未知key
 */
HAL_StatusTypeDef Config_Get(const char *key, char *buf, uint8_t bufLen);

#endif /* __CONFIG_H */
