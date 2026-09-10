/**
 * @file    fs_file.c
 * @brief   文件管理实现 - 文件创建/读/写/删除
 * @author  [Your Name]
 * @date    2025-01-01
 * @version 1.0
 */

#include "fs_file.h"
#include "fs_core.h"
#include "bsp_flash.h"
#include "utils.h"

/* ========== 私有宏定义 ========== */

/**
 * @brief 计算文件条目在索引表中的地址
 * @param index 文件索引(0 ~ FS_FILE_MAX-1)
 */
#define FS_GET_ENTRY_ADDR(index)    (FS_ADDR_INDEX_TABLE + (index) * sizeof(FileEntry_t))

/* ========== 索引缓存 ========== */

static FileEntry_t s_index_cache[FS_FILE_MAX];

static uint32_t FS_GetEntryAllocSize(const FileEntry_t *entry)
{
    uint32_t sectors;

    if (entry == NULL)
    {
        return 0U;
    }

    sectors = (uint32_t)entry->reserved[0] | ((uint32_t)entry->reserved[1] << 8);
    if (sectors != 0U)
    {
        return sectors * W25Q_SECTOR_SIZE;
    }

    return entry->size;
}

static void FS_SetEntryAllocSize(FileEntry_t *entry, uint32_t alloc_size)
{
    uint32_t sectors;

    if (entry == NULL)
    {
        return;
    }

    sectors = alloc_size / W25Q_SECTOR_SIZE;
    if (sectors > 0xFFFFU)
    {
        sectors = 0xFFFFU;
    }

    entry->reserved[0] = (uint8_t)(sectors & 0xFFU);
    entry->reserved[1] = (uint8_t)((sectors >> 8) & 0xFFU);
}

/* ========== 私有函数 ========== */

/**
 * @brief 加载索引表到内存缓存
 * @note  FS_Init 成功后调用，一次性读取全部条目
 */
void FS_LoadIndexCache(void)
{
    BSP_Flash_ReadData(FS_ADDR_INDEX_TABLE, (uint8_t *)s_index_cache,
                       sizeof(s_index_cache));
}

uint8_t FS_GetEntryType(uint8_t index)
{
    if (index >= FS_FILE_MAX)
    {
        return 0xFF;
    }
    return s_index_cache[index].type;
}

FSResult_e FS_ValidateAllocRange(uint32_t alloc_addr, uint32_t alloc_end, uint8_t this_index, uint32_t *freed_bytes)
{
    uint8_t i;

    *freed_bytes = 0;

    for (i = 0; i < FS_FILE_MAX; i++)
    {
        if (s_index_cache[i].flags != FS_FLAG_VALID) continue;
        if (s_index_cache[i].size == 0) continue;

        uint32_t file_size  = FS_GetEntryAllocSize(&s_index_cache[i]);
        uint32_t file_start = s_index_cache[i].start_addr;
        uint32_t file_end   = file_start + file_size - 1;

        /* 无重叠 */
        if (file_end < alloc_addr || file_start > alloc_end) continue;

        if (i == this_index)
        {
            /* 自身冲突：上次写入的索引已提交但超级块未提交，
               next_write_addr 尚未前进，应跳过自身数据 */
            return FS_ERR_EXIST;
        }

        /* 非LOG文件不可覆盖 */
        if (s_index_cache[i].type != FILE_TYPE_LOG)
        {
            return FS_ERR_EXIST;
        }

        /* 旧LOG文件：标记删除，累计释放字节数 */
        *freed_bytes += file_size;
        s_index_cache[i].flags = FS_FLAG_DELETED;
    }

    return FS_OK;
}

/**
 * @brief 读取索引表中的文件条目（从缓存读取）
 * @param index 文件索引
 * @param entry 条目输出
 * @return FS_OK: 成功, FS_ERR_PARAM: 参数错误
 */
#if 0
static FSResult_e FS_ReadEntry(uint8_t index, FileEntry_t *entry)
{
    if (index >= FS_FILE_MAX || entry == NULL)
    {
        return FS_ERR_PARAM;
    }

    memcpy(entry, &s_index_cache[index], sizeof(FileEntry_t));
    return FS_OK;
}

/**
 * @brief 写入文件条目到索引表
 * @param index 文件索引
 * @param entry 条目指针
 * @return FS_OK: 成功, FS_ERR_FLASH: Flash错误
 */
#endif
static FSResult_e FS_WriteEntry(uint8_t index, FileEntry_t *entry)
{
    uint32_t sector_addr;
    uint16_t i;

    if (index >= FS_FILE_MAX || entry == NULL)
    {
        return FS_ERR_PARAM;
    }

    /* 更新缓存 */
    memcpy(&s_index_cache[index], entry, sizeof(FileEntry_t));

    /* 索引表在第一个扇区内，构造写回缓冲区 */
    sector_addr = FS_ADDR_INDEX_TABLE;

    static uint8_t sector_buf[W25Q_SECTOR_SIZE];
    memset(sector_buf, 0xFF, sizeof(sector_buf));
    memcpy(sector_buf, s_index_cache, sizeof(s_index_cache));

    /* 擦除扇区 */
    if (BSP_Flash_SectorErase(sector_addr) != HAL_OK)
    {
        return FS_ERR_FLASH;
    }

    /* 分页写回 */
    for (i = 0; i < W25Q_SECTOR_SIZE / W25Q_PAGE_SIZE; i++)
    {
        if (BSP_Flash_PageWrite(sector_addr + i * W25Q_PAGE_SIZE,
                                &sector_buf[i * W25Q_PAGE_SIZE],
                                W25Q_PAGE_SIZE) != HAL_OK)
        {
            return FS_ERR_FLASH;
        }
    }

    return FS_OK;
}

/**
 * @brief 查找空闲条目槽
 * @param p_index 空闲索引输出
 * @return FS_OK: 找到, FS_ERR_FULL: 索引表满
 */
static FSResult_e FS_FindFreeSlot(uint8_t *p_index)
{
    uint8_t i;

    for (i = 0; i < FS_FILE_MAX; i++)
    {
        if (s_index_cache[i].flags != FS_FLAG_VALID)
        {
            *p_index = i;
            return FS_OK;
        }
    }

    return FS_ERR_FULL;
}

static uint32_t FS_CRC32_UpdateRaw(uint32_t crc, const uint8_t *data, uint32_t len)
{
    uint8_t bit;

    while (len-- > 0U)
    {
        crc ^= *data++;
        for (bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 1U) != 0U)
            {
                crc = (crc >> 1) ^ 0xEDB88320UL;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc;
}

static uint32_t FS_RoundUpToSector(uint32_t size)
{
    uint32_t rounded;

    if (size == 0U)
    {
        return W25Q_SECTOR_SIZE;
    }

    if (size > (0xFFFFFFFFUL - (W25Q_SECTOR_SIZE - 1U)))
    {
        return 0U;
    }

    rounded = ((size + W25Q_SECTOR_SIZE - 1U) / W25Q_SECTOR_SIZE) * W25Q_SECTOR_SIZE;
    if (rounded == 0U)
    {
        rounded = W25Q_SECTOR_SIZE;
    }

    return rounded;
}

static FSResult_e FS_PageSafeWrite(uint32_t addr, const uint8_t *data, uint32_t len)
{
    uint32_t remaining = len;
    uint32_t written = 0U;
    uint32_t page_space;
    uint16_t chunk_size;

    while (remaining > 0U)
    {
        page_space = W25Q_PAGE_SIZE - (addr % W25Q_PAGE_SIZE);
        chunk_size = (remaining > page_space) ? (uint16_t)page_space : (uint16_t)remaining;

        if (BSP_Flash_PageWrite(addr, (uint8_t *)(data + written), chunk_size) != HAL_OK)
        {
            return FS_ERR_FLASH;
        }

        addr += chunk_size;
        written += chunk_size;
        remaining -= chunk_size;
    }

    return FS_OK;
}

static FSResult_e FS_IsErasedRange(uint32_t addr, uint32_t len, bool *is_erased)
{
    uint8_t buf[64];
    uint32_t remaining = len;
    uint32_t chunk_size;
    uint32_t i;

    if (is_erased == NULL)
    {
        return FS_ERR_PARAM;
    }

    *is_erased = true;

    while (remaining > 0U)
    {
        chunk_size = (remaining > sizeof(buf)) ? (uint32_t)sizeof(buf) : remaining;

        if (BSP_Flash_ReadData(addr, buf, chunk_size) != HAL_OK)
        {
            return FS_ERR_FLASH;
        }

        for (i = 0U; i < chunk_size; i++)
        {
            if (buf[i] != 0xFFU)
            {
                *is_erased = false;
                return FS_OK;
            }
        }

        addr += chunk_size;
        remaining -= chunk_size;
    }

    return FS_OK;
}

static FSResult_e FS_CRC32_UpdateFromFlash(uint32_t addr, uint32_t len, uint32_t *crc)
{
    uint8_t buf[64];
    uint32_t remaining = len;
    uint32_t chunk_size;

    if (crc == NULL)
    {
        return FS_ERR_PARAM;
    }

    while (remaining > 0U)
    {
        chunk_size = (remaining > sizeof(buf)) ? (uint32_t)sizeof(buf) : remaining;

        if (BSP_Flash_ReadData(addr, buf, chunk_size) != HAL_OK)
        {
            return FS_ERR_FLASH;
        }

        *crc = FS_CRC32_UpdateRaw(*crc, buf, chunk_size);
        addr += chunk_size;
        remaining -= chunk_size;
    }

    return FS_OK;
}

/* ========== 公共函数实现 ========== */

/**
 * @brief 创建新文件
 */
FSResult_e FS_File_Create(const char *name, FileType_e type)
{
    FileEntry_t entry;
    FileEntry_t existing;
    uint8_t free_index;
    FSResult_e result;

    /* 参数检查 */
    if (name == NULL || strlen(name) == 0 || strlen(name) >= FS_FILENAME_MAX)
    {
        return FS_ERR_PARAM;
    }

    /* 检查文件是否已存在 */
    if (FS_File_Find(name, &existing, NULL) == FS_OK)
    {
        return FS_ERR_EXIST;
    }

    /* 查找空闲槽 */
    result = FS_FindFreeSlot(&free_index);
    if (result != FS_OK)
    {
        return result;
    }

    /* 初始化文件条目 */
    memset(&entry, 0, sizeof(FileEntry_t));
    strncpy(entry.name, name, FS_FILENAME_MAX - 1);
    entry.name[FS_FILENAME_MAX - 1] = '\0';
    entry.start_addr = 0;
    entry.size = 0;
    entry.crc = 0;
    entry.flags = FS_FLAG_VALID;
    entry.type = (uint8_t)type;

    /* 写入索引表 */
    if (FS_WriteEntry(free_index, &entry) != FS_OK)
    {
        return FS_ERR_FLASH;
    }

    FS_AdjustFileCount(1);
    if (FS_FlushSuperBlock() != FS_OK)
    {
        return FS_ERR_FLASH;
    }

    return FS_OK;
}

/**
 * @brief 写入文件数据
 */
FSResult_e FS_File_Write(const char *name, const uint8_t *data, uint32_t len)
{
    FileEntry_t entry;
    uint8_t index;
    uint32_t alloc_addr;
    uint32_t written;
    uint32_t remaining;
    uint32_t write_addr;
    uint16_t write_size;

    /* 参数检查 */
    if (name == NULL || (data == NULL && len > 0))
    {
        return FS_ERR_PARAM;
    }

    /* 查找文件 */
    if (FS_File_Find(name, &entry, &index) != FS_OK)
    {
        return FS_ERR_NOT_FOUND;
    }

    /* 分配数据空间 */
    alloc_addr = FS_AllocDataBlock(len, index);
    if (alloc_addr == 0 && len > 0)
    {
        return FS_ERR_NO_SPACE;
    }

    /* 写入数据（分页写入） */
    write_addr = alloc_addr;
    remaining = len;
    written = 0;

    while (remaining > 0)
    {
        write_size = (remaining > W25Q_PAGE_SIZE) ? W25Q_PAGE_SIZE : remaining;

        if (BSP_Flash_PageWrite(write_addr, (uint8_t *)(data + written), write_size) != HAL_OK)
        {
            return FS_ERR_FLASH;
        }

        write_addr += write_size;
        written += write_size;
        remaining -= write_size;
    }

    /* 更新文件条目 */
    entry.start_addr = alloc_addr;
    entry.size = len;
    FS_SetEntryAllocSize(&entry, 0U);
    entry.crc = Utils_CRC32_Calc(data, len);

    /* 写回索引表（数据已落盘，索引为提交点） */
    if (FS_WriteEntry(index, &entry) != FS_OK)
    {
        return FS_ERR_FLASH;
    }

    /* 索引持久化后提交超级块（最后一步） */
    if (FS_FlushSuperBlock() != FS_OK)
    {
        return FS_ERR_FLASH;
    }

    return FS_OK;
}

/**
 * @brief Append data to an existing file.
 */
FSResult_e FS_File_Append(const char *name, const uint8_t *data, uint32_t len)
{
    FileEntry_t entry;
    uint8_t index = 0U;
    uint8_t copy_buf[64];
    uint32_t old_size;
    uint32_t new_size;
    uint32_t capacity;
    uint32_t alloc_size;
    uint32_t new_addr;
    uint32_t read_addr;
    uint32_t write_addr;
    uint32_t remaining;
    uint32_t crc = 0xFFFFFFFFUL;
    uint32_t old_crc;
    uint16_t chunk_size;
    FSResult_e ret;
    bool is_erased;

    if (name == NULL || data == NULL || len == 0U)
    {
        return FS_ERR_PARAM;
    }

    ret = FS_File_Find(name, &entry, &index);
    if (ret != FS_OK)
    {
        return ret;
    }

    old_size = entry.size;
    new_size = old_size + len;
    if (new_size < old_size)
    {
        return FS_ERR_NO_SPACE;
    }

    alloc_size = FS_RoundUpToSector(new_size);
    if (alloc_size == 0U)
    {
        return FS_ERR_NO_SPACE;
    }

    if (old_size == 0U || entry.start_addr == 0U)
    {
        new_addr = FS_AllocDataBlock(alloc_size, index);
        if (new_addr == 0U)
        {
            return FS_ERR_NO_SPACE;
        }

        ret = FS_PageSafeWrite(new_addr, data, len);
        if (ret != FS_OK)
        {
            return ret;
        }

        entry.start_addr = new_addr;
        entry.size = len;
        FS_SetEntryAllocSize(&entry, alloc_size);
        entry.crc = Utils_CRC32_Calc(data, len);

        ret = FS_WriteEntry(index, &entry);
        if (ret != FS_OK)
        {
            return ret;
        }

        ret = FS_FlushSuperBlock();
        if (ret != FS_OK)
        {
            return ret;
        }

        return FS_OK;
    }

    ret = FS_CRC32_UpdateFromFlash(entry.start_addr, old_size, &crc);
    if (ret != FS_OK)
    {
        return ret;
    }

    old_crc = crc ^ 0xFFFFFFFFUL;
    if (old_crc != entry.crc)
    {
        return FS_ERR_CRC;
    }

    capacity = FS_RoundUpToSector(old_size);
    if (capacity == 0U)
    {
        return FS_ERR_NO_SPACE;
    }

    if (new_size <= capacity)
    {
        ret = FS_IsErasedRange(entry.start_addr + old_size, len, &is_erased);
        if (ret != FS_OK)
        {
            return ret;
        }

        if (is_erased)
        {
            crc = FS_CRC32_UpdateRaw(crc, data, len);

            ret = FS_PageSafeWrite(entry.start_addr + old_size, data, len);
            if (ret != FS_OK)
            {
                return ret;
            }

            entry.size = new_size;
            entry.crc = crc ^ 0xFFFFFFFFUL;

            ret = FS_WriteEntry(index, &entry);
            if (ret != FS_OK)
            {
                return ret;
            }

            ret = FS_FlushSuperBlock();
            if (ret != FS_OK)
            {
                return ret;
            }

            return FS_OK;
        }
    }

    new_addr = FS_AllocDataBlock(alloc_size, index);
    if (new_addr == 0U)
    {
        return FS_ERR_NO_SPACE;
    }

    read_addr = entry.start_addr;
    write_addr = new_addr;
    remaining = old_size;
    crc = 0xFFFFFFFFUL;

    while (remaining > 0U)
    {
        chunk_size = (remaining > sizeof(copy_buf)) ? (uint16_t)sizeof(copy_buf) : (uint16_t)remaining;

        if (BSP_Flash_ReadData(read_addr, copy_buf, chunk_size) != HAL_OK)
        {
            return FS_ERR_FLASH;
        }

        ret = FS_PageSafeWrite(write_addr, copy_buf, chunk_size);
        if (ret != FS_OK)
        {
            return ret;
        }

        crc = FS_CRC32_UpdateRaw(crc, copy_buf, chunk_size);
        read_addr += chunk_size;
        write_addr += chunk_size;
        remaining -= chunk_size;
    }

    ret = FS_PageSafeWrite(write_addr, data, len);
    if (ret != FS_OK)
    {
        return ret;
    }
    crc = FS_CRC32_UpdateRaw(crc, data, len);

    entry.start_addr = new_addr;
    entry.size = new_size;
    FS_SetEntryAllocSize(&entry, alloc_size);
    entry.crc = crc ^ 0xFFFFFFFFUL;

    ret = FS_WriteEntry(index, &entry);
    if (ret != FS_OK)
    {
        return ret;
    }

    FS_SubtractDataUsage(capacity);

    ret = FS_FlushSuperBlock();
    if (ret != FS_OK)
    {
        return ret;
    }

    return FS_OK;
}

FSResult_e FS_File_AppendReserved(const char *name, const uint8_t *data, uint32_t len, uint32_t reserve_len)
{
    FileEntry_t entry;
    uint8_t index = 0U;
    uint32_t old_size;
    uint32_t new_size;
    uint32_t capacity;
    uint32_t alloc_size;
    uint32_t new_addr;
    uint32_t crc = 0xFFFFFFFFUL;
    uint32_t old_crc;
    FSResult_e ret;
    bool is_erased;

    if (name == NULL || data == NULL || len == 0U)
    {
        return FS_ERR_PARAM;
    }

    alloc_size = FS_RoundUpToSector(reserve_len);
    if (alloc_size == 0U || len > alloc_size || alloc_size > FS_DATA_SIZE)
    {
        return FS_ERR_PARAM;
    }

    ret = FS_File_Find(name, &entry, &index);
    if (ret != FS_OK)
    {
        return ret;
    }

    old_size = entry.size;
    capacity = FS_GetEntryAllocSize(&entry);
    if (capacity == 0U)
    {
        capacity = alloc_size;
    }
    new_size = old_size + len;
    if (new_size < old_size || new_size > capacity)
    {
        return FS_ERR_NO_SPACE;
    }

    if (old_size == 0U || entry.start_addr == 0U)
    {
        new_addr = FS_AllocDataBlock(alloc_size, index);
        if (new_addr == 0U)
        {
            return FS_ERR_NO_SPACE;
        }

        ret = FS_PageSafeWrite(new_addr, data, len);
        if (ret != FS_OK)
        {
            return ret;
        }

        entry.start_addr = new_addr;
        entry.size = len;
        FS_SetEntryAllocSize(&entry, alloc_size);
        entry.crc = Utils_CRC32_Calc(data, len);

        ret = FS_WriteEntry(index, &entry);
        if (ret != FS_OK)
        {
            return ret;
        }

        ret = FS_FlushSuperBlock();
        if (ret != FS_OK)
        {
            return ret;
        }

        return FS_OK;
    }

    ret = FS_CRC32_UpdateFromFlash(entry.start_addr, old_size, &crc);
    if (ret != FS_OK)
    {
        return ret;
    }

    old_crc = crc ^ 0xFFFFFFFFUL;
    if (old_crc != entry.crc)
    {
        return FS_ERR_CRC;
    }

    ret = FS_IsErasedRange(entry.start_addr + old_size, len, &is_erased);
    if (ret != FS_OK)
    {
        return ret;
    }
    if (!is_erased)
    {
        return FS_ERR_FLASH;
    }

    crc = FS_CRC32_UpdateRaw(crc, data, len);
    ret = FS_PageSafeWrite(entry.start_addr + old_size, data, len);
    if (ret != FS_OK)
    {
        return ret;
    }

    entry.size = new_size;
    entry.crc = crc ^ 0xFFFFFFFFUL;

    ret = FS_WriteEntry(index, &entry);
    if (ret != FS_OK)
    {
        return ret;
    }

    ret = FS_FlushSuperBlock();
    if (ret != FS_OK)
    {
        return ret;
    }

    return FS_OK;
}

FSResult_e FS_File_RepairPrefix(const char *name, uint32_t valid_size)
{
    FileEntry_t entry;
    uint8_t index = 0U;
    uint32_t crc;
    FSResult_e ret;

    if (name == NULL)
    {
        return FS_ERR_PARAM;
    }

    ret = FS_File_Find(name, &entry, &index);
    if (ret != FS_OK)
    {
        return ret;
    }
    if (valid_size == 0U || valid_size > entry.size || entry.start_addr == 0U)
    {
        return FS_ERR_PARAM;
    }

    crc = 0xFFFFFFFFUL;
    ret = FS_CRC32_UpdateFromFlash(entry.start_addr, valid_size, &crc);
    if (ret != FS_OK)
    {
        return ret;
    }
    crc ^= 0xFFFFFFFFUL;

    if (entry.size == valid_size && entry.crc == crc)
    {
        return FS_OK;
    }

    entry.size = valid_size;
    entry.crc = crc;

    ret = FS_WriteEntry(index, &entry);
    if (ret != FS_OK)
    {
        return ret;
    }

    return FS_FlushSuperBlock();
}

/**
 * @brief Read file data.
 */
FSResult_e FS_File_Read(const char *name, uint8_t *buf, uint32_t buf_len, uint32_t *read_len)
{
    FileEntry_t entry;
    uint32_t actual_len;
    uint32_t calc_crc;

    /* 参数检查 */
    if (name == NULL || buf == NULL || read_len == NULL)
    {
        return FS_ERR_PARAM;
    }

    /* 查找文件 */
    if (FS_File_Find(name, &entry, NULL) != FS_OK)
    {
        return FS_ERR_NOT_FOUND;
    }

    /* 计算实际读取长度 */
    actual_len = (entry.size < buf_len) ? entry.size : buf_len;
    *read_len = actual_len;

    /* 文件大小为0 */
    if (actual_len == 0)
    {
        return FS_OK;
    }

    /* 读取数据 */
    if (BSP_Flash_ReadData(entry.start_addr, buf, actual_len) != HAL_OK)
    {
        return FS_ERR_FLASH;
    }

    /* CRC校验（读取全部数据） */
    if (entry.size > 0)
    {
        static uint8_t verify_buf[256];    /* 临时缓冲区用于CRC校验 */
        uint32_t remaining = entry.size;
        uint32_t read_addr = entry.start_addr;
        uint32_t chunk_size;

        calc_crc = 0xFFFFFFFF;    /* 初始值 */

        while (remaining > 0)
        {
            chunk_size = (remaining > sizeof(verify_buf)) ? sizeof(verify_buf) : remaining;

            if (BSP_Flash_ReadData(read_addr, verify_buf, chunk_size) != HAL_OK)
            {
                return FS_ERR_FLASH;
            }

            /* 累积CRC计算需要特殊处理，这里简化为一次性计算 */
            /* 实际应该分块累积，但为简化起见，读取全部数据后计算 */
            remaining -= chunk_size;
            read_addr += chunk_size;
        }

        /* 重新计算CRC */
        /* 由于缓冲区限制，这里只校验读取的部分 */
        calc_crc = Utils_CRC32_Calc(buf, actual_len);

        /* 如果读取了全部数据，进行完整CRC校验 */
        if (actual_len == entry.size && calc_crc != entry.crc)
        {
            return FS_ERR_CRC;
        }
    }

    return FS_OK;
}

/**
 * @brief 删除文件
 */
FSResult_e FS_File_ReadRange(const char *name, uint32_t offset, uint8_t *buf, uint32_t len)
{
    FileEntry_t entry;

    if (name == NULL || buf == NULL)
    {
        return FS_ERR_PARAM;
    }

    if (len == 0U)
    {
        return FS_OK;
    }

    if (FS_File_Find(name, &entry, NULL) != FS_OK)
    {
        return FS_ERR_NOT_FOUND;
    }

    if (offset > entry.size || len > (entry.size - offset))
    {
        return FS_ERR_PARAM;
    }

    if (BSP_Flash_ReadData(entry.start_addr + offset, buf, len) != HAL_OK)
    {
        return FS_ERR_FLASH;
    }

    return FS_OK;
}

FSResult_e FS_File_Delete(const char *name)
{
    FileEntry_t entry;
    uint8_t index;
    uint32_t old_alloc_size;

    /* 参数检查 */
    if (name == NULL)
    {
        return FS_ERR_PARAM;
    }

    /* 查找文件 */
    if (FS_File_Find(name, &entry, &index) != FS_OK)
    {
        return FS_ERR_NOT_FOUND;
    }
    old_alloc_size = FS_GetEntryAllocSize(&entry);

    /* 标记为已删除 */
    entry.flags = FS_FLAG_DELETED;

    /* 写回索引表 */
    if (FS_WriteEntry(index, &entry) != FS_OK)
    {
        return FS_ERR_FLASH;
    }

    FS_AdjustFileCount(-1);
    FS_SubtractDataUsage(old_alloc_size);
    if (FS_FlushSuperBlock() != FS_OK)
    {
        return FS_ERR_FLASH;
    }

    return FS_OK;
}

/**
 * @brief 列出所有有效文件
 */
FSResult_e FS_File_List(FileEntry_t *list, uint8_t max_count, uint8_t *actual_count)
{
    uint8_t i;
    uint8_t count = 0;

    /* 参数检查 */
    if (list == NULL || actual_count == NULL)
    {
        return FS_ERR_PARAM;
    }

    *actual_count = 0;

    for (i = 0; i < FS_FILE_MAX && count < max_count; i++)
    {
        if (s_index_cache[i].flags == FS_FLAG_VALID)
        {
            memcpy(&list[count], &s_index_cache[i], sizeof(FileEntry_t));
            count++;
        }
    }

    *actual_count = count;

    return FS_OK;
}

/**
 * @brief 查找文件
 */
FSResult_e FS_File_Find(const char *name, FileEntry_t *entry, uint8_t *index)
{
    uint8_t i;

    /* 参数检查 */
    if (name == NULL || entry == NULL)
    {
        return FS_ERR_PARAM;
    }

    for (i = 0; i < FS_FILE_MAX; i++)
    {
        if (s_index_cache[i].flags == FS_FLAG_VALID &&
            strncmp(s_index_cache[i].name, name, FS_FILENAME_MAX) == 0)
        {
            memcpy(entry, &s_index_cache[i], sizeof(FileEntry_t));

            if (index != NULL)
            {
                *index = i;
            }

            return FS_OK;
        }
    }

    return FS_ERR_NOT_FOUND;
}

/**
 * @brief 获取文件大小
 */
uint32_t FS_File_GetSize(const char *name)
{
    FileEntry_t entry;

    if (FS_File_Find(name, &entry, NULL) == FS_OK)
    {
        return entry.size;
    }

    return 0;
}
