/**
 * @file    fs_core.h
 * @brief   文件系统核心管理 - 超级块与空间分配
 * @author  [Your Name]
 * @date    2025-01-01
 * @version 1.0
 */

#ifndef __FS_CORE_H
#define __FS_CORE_H

#include "fs_types.h"

/* ========== 函数声明 ========== */

/**
 * @brief 初始化文件系统
 * @return FS_OK: 成功, FS_ERR_NOT_INIT: 未格式化或CRC错误, FS_ERR_FLASH: Flash错误
 * @note  读取超级块，验证magic和CRC，加载到内存
 */
FSResult_e FS_Init(void);

/**
 * @brief 格式化文件系统
 * @return FS_OK: 成功, FS_ERR_FLASH: Flash操作失败
 * @note  擦除超级块区和索引表区，写入初始超级块
 */
FSResult_e FS_Format(void);

/**
 * @brief 获取文件系统状态
 * @param used_bytes  已使用字节数输出
 * @param total_bytes 总容量字节数输出
 */
void FS_GetStatus(uint32_t *used_bytes, uint32_t *total_bytes);
uint32_t FS_GetFileCount(void);
void FS_AdjustFileCount(int32_t delta);
void FS_SubtractDataUsage(uint32_t bytes);

/**
 * @brief 分配数据空间
 * @param size 需要分配的字节数
 * @return 分配的起始地址，若失败返回0
 * @note  从next_write_addr分配，支持循环写入，自动擦除目标扇区
 */
uint32_t FS_AllocDataBlock(uint32_t size, uint8_t this_index);

/**
 * @brief 持久化超级块到Flash
 * @return FS_OK: 成功, FS_ERR_FLASH: Flash操作失败
 */
FSResult_e FS_FlushSuperBlock(void);

/**
 * @brief 检查文件系统是否已初始化
 * @return 1: 已初始化, 0: 未初始化
 */
uint8_t FS_IsInitialized(void);

#endif /* __FS_CORE_H */
