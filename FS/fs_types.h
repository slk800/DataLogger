/**
 * @file    fs_types.h
 * @brief   文件系统类型定义 - 数据结构与常量
 * @author  [Your Name]
 * @date    2025-01-01
 * @version 1.0
 */

#ifndef __FS_TYPES_H
#define __FS_TYPES_H

#include <stdint.h>

/* ========== Flash 地址空间规划 ========== */
#define FS_ADDR_SUPER_BLOCK     0x000000    /* 超级块区起始地址 (4KB) */
#define FS_ADDR_INDEX_TABLE     0x001000    /* 文件索引表区起始地址 (12KB) */
#define FS_ADDR_CONFIG_A        0x004000    /* CONFIG_A区起始地址 (8KB) */
#define FS_ADDR_CONFIG_B        0x006000    /* CONFIG_B区起始地址 (8KB) */
#define FS_ADDR_DATA_START      0x008000    /* DATA/LOG区起始地址 */
#define FS_ADDR_DATA_END        0x7FFFFF    /* DATA/LOG区结束地址 */

/* ========== 区域大小定义 ========== */
#define FS_SUPER_BLOCK_SIZE     0x001000    /* 超级块区大小 4KB */
#define FS_INDEX_TABLE_SIZE     0x003000    /* 索引表区大小 12KB */
#define FS_CONFIG_SIZE          0x002000    /* CONFIG区大小 8KB */
#define FS_DATA_SIZE            (FS_ADDR_DATA_END - FS_ADDR_DATA_START + 1)  /* 数据区大小 */

/* ========== 文件系统常量 ========== */
#define FS_MAGIC                0xDEADBEEF  /* 超级块魔数 */
#define FS_FILE_MAX             32          /* 最大文件数 */
#define FS_FILENAME_MAX         16          /* 文件名最大长度(含'\0') */
#define FS_VERSION              1           /* 文件系统版本号 */

/* ========== 文件条目标志 ========== */
#define FS_FLAG_VALID           0xAA        /* 文件有效标志 */
#define FS_FLAG_DELETED         0x00        /* 文件已删除标志 */

/* ========== 枚举类型定义 ========== */

/**
 * @brief 文件类型枚举
 */
typedef enum {
    FILE_TYPE_LOG    = 0,    /* 日志文件 */
    FILE_TYPE_CONFIG = 1,    /* 配置文件 */
    FILE_TYPE_DATA   = 2     /* 数据文件 */
} FileType_e;

/**
 * @brief 文件系统操作结果枚举
 */
typedef enum {
    FS_OK               =  0,    /* 操作成功 */
    FS_ERR_NOT_INIT     = -1,    /* 文件系统未初始化 */
    FS_ERR_NOT_FOUND    = -2,    /* 文件未找到 */
    FS_ERR_FULL         = -3,    /* 文件系统已满 */
    FS_ERR_EXIST        = -4,    /* 文件已存在 */
    FS_ERR_CRC          = -5,    /* CRC校验失败 */
    FS_ERR_FLASH        = -6,    /* Flash操作失败 */
    FS_ERR_PARAM        = -7,    /* 参数错误 */
    FS_ERR_NO_SPACE     = -8     /* 空间不足 */
} FSResult_e;

/* ========== 结构体定义 ========== */

/**
 * @brief 超级块结构体 (24字节)
 * @note  存储于Flash地址 0x000000
 */
typedef struct {
    uint32_t magic;             /* 魔数，用于验证文件系统有效性 */
    uint32_t version;           /* 文件系统版本号 */
    uint32_t file_count;        /* 当前有效文件数量 */
    uint32_t next_write_addr;   /* 下一次写入的起始地址(数据区) */
    uint32_t total_data_size;   /* 已写入的总数据量(统计用) */
    uint32_t crc;               /* 超级块CRC32校验值 */
} SuperBlock_t;

/**
 * @brief 文件索引条目结构体 (32字节)
 * @note  存储于Flash地址 0x001000 开始，每个条目32字节
 */
typedef struct {
    char     name[FS_FILENAME_MAX]; /* 文件名 */
    uint32_t start_addr;            /* 文件数据起始地址(数据区) */
    uint32_t size;                  /* 文件大小(字节) */
    uint32_t crc;                   /* 文件数据CRC32校验值 */
    uint8_t  flags;                 /* 状态标志: 0xAA=有效, 0x00=已删除 */
    uint8_t  type;                  /* 文件类型(FileType_e) */
    uint8_t  reserved[2];           /* 保留字段 */
} FileEntry_t;

#endif /* __FS_TYPES_H */
