/**
 * @file    fs_file.h
 * @brief   文件管理接口 - 文件创建/读/写/删除
 * @author  [Your Name]
 * @date    2025-01-01
 * @version 1.0
 */

#ifndef __FS_FILE_H
#define __FS_FILE_H

#include "fs_types.h"

/* ========== 函数声明 ========== */

/**
 * @brief 创建新文件
 * @param name 文件名
 * @param type 文件类型
 * @return FS_OK: 成功, FS_ERR_EXIST: 文件已存在, FS_ERR_FULL: 索引表满
 */
FSResult_e FS_File_Create(const char *name, FileType_e type);

/**
 * @brief 写入文件数据
 * @param name 文件名
 * @param data 数据指针
 * @param len  数据长度
 * @return FS_OK: 成功, FS_ERR_NOT_FOUND: 文件不存在, FS_ERR_FLASH: Flash错误
 * @note  会覆盖原有数据，重新分配空间
 */
FSResult_e FS_File_Write(const char *name, const uint8_t *data, uint32_t len);
FSResult_e FS_File_Append(const char *name, const uint8_t *data, uint32_t len);
FSResult_e FS_File_AppendReserved(const char *name, const uint8_t *data, uint32_t len, uint32_t reserve_len);
FSResult_e FS_File_RepairPrefix(const char *name, uint32_t valid_size);

/**
 * @brief 读取文件数据
 * @param name     文件名
 * @param buf      数据缓冲区
 * @param buf_len  缓冲区大小
 * @param read_len 实际读取字节数输出
 * @return FS_OK: 成功, FS_ERR_NOT_FOUND: 文件不存在, FS_ERR_CRC: CRC校验失败
 */
FSResult_e FS_File_Read(const char *name, uint8_t *buf, uint32_t buf_len, uint32_t *read_len);
FSResult_e FS_File_ReadRange(const char *name, uint32_t offset, uint8_t *buf, uint32_t len);

/**
 * @brief 删除文件
 * @param name 文件名
 * @return FS_OK: 成功, FS_ERR_NOT_FOUND: 文件不存在
 */
FSResult_e FS_File_Delete(const char *name);

/**
 * @brief 列出所有有效文件
 * @param list        文件条目数组
 * @param max_count   数组最大容量
 * @param actual_count 实际文件数输出
 * @return FS_OK: 成功
 */
FSResult_e FS_File_List(FileEntry_t *list, uint8_t max_count, uint8_t *actual_count);
void       FS_LoadIndexCache(void);
uint8_t    FS_GetEntryType(uint8_t index);
FSResult_e FS_ValidateAllocRange(uint32_t alloc_addr, uint32_t alloc_end, uint8_t this_index, uint32_t *freed_bytes);

/**
 * @brief 查找文件
 * @param name 文件名
 * @param entry 文件条目输出
 * @param index 文件索引输出（可选，传NULL忽略）
 * @return FS_OK: 找到, FS_ERR_NOT_FOUND: 未找到
 */
FSResult_e FS_File_Find(const char *name, FileEntry_t *entry, uint8_t *index);

/**
 * @brief 获取文件大小
 * @param name 文件名
 * @return 文件大小，若文件不存在返回0
 */
uint32_t FS_File_GetSize(const char *name);

#endif /* __FS_FILE_H */
