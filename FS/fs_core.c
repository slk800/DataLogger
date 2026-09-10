/**
 * @file    fs_core.c
 * @brief   文件系统核心管理实现 - 超级块与空间分配
 * @author  [Your Name]
 * @date    2025-01-01
 * @version 1.0
 *
 * ========== 磨损均衡策略说明 ==========
 *
 * 当前实现为简化版磨损均衡：
 *
 * 数据区（0x008000 ~ 0x7FFFFF, ~8MB）：
 *   LOG 文件采用循环写入（append-only ring buffer），
 *   写入指针顺序推进，回绕后覆盖最旧数据。
 *   数据区大（8MB），单个扇区擦写频率低，磨损分散。
 *
 * 已知限制：
 *   1. 超级块扇区（0x000000）—— 每次 FS_FlushSuperBlock 擦写一次，
 *      每笔 LOG 写入触发一次。W25Q64 典型耐久 100K 次，
 *      按 1 条/秒计，约 27 小时耗尽。
 *      改进方向：超级块写缓存（批量提交，减少擦写频率）。
 *   2. 索引表扇区（0x001000）—— 每次 FS_WriteEntry 擦写一次。
 *      同上，写入频率等于 LOG 写入频率。
 *      改进方向：增量写入（不在同一位置反复擦写）。
 *   3. CONFIG 双区（0x004000/0x006000）—— Config_Save 每次交替擦写，
 *      已通过双区冗余降低单区磨损。写入频率低（仅配置变更时）。
 */

#include "fs_core.h"
#include "fs_file.h"
#include "bsp_flash.h"
#include "utils.h"

/* ========== 私有变量 ========== */

/**
 * @brief 内存中的超级块副本
 * @note  所有操作先修改此副本，需要时持久化到Flash
 */
static SuperBlock_t s_superblock;

/**
 * @brief 文件系统初始化标志
 */
static uint8_t s_initialized = 0;

/* ========== 私有函数 ========== */

/**
 * @brief 计算超级块CRC（不含crc字段本身）
 * @param sb 超级块指针
 * @return CRC32值
 */
static uint32_t FS_CalcSuperBlockCRC(SuperBlock_t *sb)
{
    /* 计算前20字节的CRC（不含最后4字节crc字段） */
    return Utils_CRC32_Calc((uint8_t *)sb, sizeof(SuperBlock_t) - sizeof(uint32_t));
}

/**
 * @brief 擦除指定范围覆盖的所有扇区
 * @param start_addr 起始地址
 * @param end_addr   结束地址
 * @return FS_OK: 成功, FS_ERR_FLASH: 失败
 */
static FSResult_e FS_EraseRange(uint32_t start_addr, uint32_t end_addr)
{
    uint32_t addr;

    /* 逐扇区擦除 */
    for (addr = start_addr; addr <= end_addr; addr += W25Q_SECTOR_SIZE)
    {
        if (BSP_Flash_SectorErase(addr) != HAL_OK)
        {
            return FS_ERR_FLASH;
        }
    }

    return FS_OK;
}

/* ========== 公共函数实现 ========== */

/**
 * @brief 初始化文件系统
 */
FSResult_e FS_Init(void)
{
    uint8_t read_buf[sizeof(SuperBlock_t)];
    uint32_t calc_crc;

    /* 读取超级块 */
    if (BSP_Flash_ReadData(FS_ADDR_SUPER_BLOCK, read_buf, sizeof(SuperBlock_t)) != HAL_OK)
    {
        return FS_ERR_FLASH;
    }

    /* 复制到内存结构体 */
    memcpy(&s_superblock, read_buf, sizeof(SuperBlock_t));

    /* 验证魔数 */
    if (s_superblock.magic != FS_MAGIC)
    {
        s_initialized = 0;
        return FS_ERR_NOT_INIT;
    }

    /* 验证CRC */
    calc_crc = FS_CalcSuperBlockCRC(&s_superblock);
    if (calc_crc != s_superblock.crc)
    {
        s_initialized = 0;
        return FS_ERR_CRC;
    }

    /* 初始化成功 */
    s_initialized = 1;
    FS_LoadIndexCache();
    return FS_OK;
}

/**
 * @brief 格式化文件系统
 */
FSResult_e FS_Format(void)
{
    FSResult_e result;

    /* 擦除超级块区 (1个扇区) */
    result = FS_EraseRange(FS_ADDR_SUPER_BLOCK, FS_ADDR_SUPER_BLOCK + FS_SUPER_BLOCK_SIZE - 1);
    if (result != FS_OK)
    {
        return result;
    }

    /* 擦除索引表区 (3个扇区) */
    result = FS_EraseRange(FS_ADDR_INDEX_TABLE, FS_ADDR_INDEX_TABLE + FS_INDEX_TABLE_SIZE - 1);
    if (result != FS_OK)
    {
        return result;
    }

    /* 初始化超级块 */
    memset(&s_superblock, 0, sizeof(SuperBlock_t));
    s_superblock.magic = FS_MAGIC;
    s_superblock.version = FS_VERSION;
    s_superblock.file_count = 0;
    s_superblock.next_write_addr = FS_ADDR_DATA_START;
    s_superblock.total_data_size = 0;

    /* 计算并填入CRC */
    s_superblock.crc = FS_CalcSuperBlockCRC(&s_superblock);

    /* 写入超级块 */
    if (BSP_Flash_PageWrite(FS_ADDR_SUPER_BLOCK, (uint8_t *)&s_superblock, sizeof(SuperBlock_t)) != HAL_OK)
    {
        return FS_ERR_FLASH;
    }

    /* 标记为已初始化 */
    s_initialized = 1;

    return FS_OK;
}

/**
 * @brief 获取文件系统状态
 */
void FS_GetStatus(uint32_t *used_bytes, uint32_t *total_bytes)
{
    if (used_bytes != NULL)
    {
        *used_bytes = s_superblock.total_data_size;
    }

    if (total_bytes != NULL)
    {
        *total_bytes = FS_DATA_SIZE;
    }
}

uint32_t FS_GetFileCount(void)
{
    if (s_initialized == 0U)
    {
        return 0U;
    }
    return s_superblock.file_count;
}

void FS_AdjustFileCount(int32_t delta)
{
    if (s_initialized == 0U)
    {
        return;
    }

    if (delta < 0)
    {
        uint32_t dec = (uint32_t)(-delta);
        s_superblock.file_count = (s_superblock.file_count > dec) ? (s_superblock.file_count - dec) : 0U;
    }
    else
    {
        s_superblock.file_count += (uint32_t)delta;
    }
}

void FS_SubtractDataUsage(uint32_t bytes)
{
    if (s_initialized == 0U)
    {
        return;
    }

    s_superblock.total_data_size = (s_superblock.total_data_size > bytes) ?
                                   (s_superblock.total_data_size - bytes) : 0U;
}

/**
 * @brief 分配数据空间（类型感知 + 冲突检测）
 * @param size       需要分配的字节数
 * @param this_index 当前文件的索引槽（用于冲突时排除自身）
 * @return 分配的起始地址，若失败返回0
 */
uint32_t FS_AllocDataBlock(uint32_t size, uint8_t this_index)
{
    uint32_t alloc_addr;
    uint32_t end_addr;
    uint32_t sector_start;
    uint32_t sector_end;
    uint32_t erase_addr;
    uint32_t freed_bytes;
    uint32_t new_total;
    uint8_t  file_type;
    uint8_t  retry;

    if (!s_initialized || size == 0 || size > FS_DATA_SIZE)
    {
        return 0;
    }

    file_type = FS_GetEntryType(this_index);

    alloc_addr = s_superblock.next_write_addr;
    end_addr   = alloc_addr + size - 1;

    /* 回绕处理：仅 LOG 类型允许循环写入 */
    if (end_addr > FS_ADDR_DATA_END)
    {
        if (file_type != FILE_TYPE_LOG)
        {
            return 0;
        }
        alloc_addr = FS_ADDR_DATA_START;
        end_addr   = alloc_addr + size - 1;
    }

    /* 冲突扫描：擦除前检查目标扇区是否覆盖其他有效文件 */
    sector_start = alloc_addr & ~(W25Q_SECTOR_SIZE - 1);
    sector_end   = end_addr | (W25Q_SECTOR_SIZE - 1);

    retry = 0;
    while (FS_ValidateAllocRange(sector_start, sector_end, this_index, &freed_bytes) != FS_OK)
    {
        if (file_type != FILE_TYPE_LOG || retry >= 4)
        {
            return 0;
        }
        /* LOG 回绕时跳过被占用扇区 */
        alloc_addr   = (sector_end + 1) & ~(W25Q_SECTOR_SIZE - 1);
        end_addr     = alloc_addr + size - 1;
        sector_start = alloc_addr & ~(W25Q_SECTOR_SIZE - 1);
        sector_end   = end_addr | (W25Q_SECTOR_SIZE - 1);
        if (alloc_addr > FS_ADDR_DATA_END)
        {
            alloc_addr = FS_ADDR_DATA_START;
            end_addr   = alloc_addr + size - 1;
            sector_start = alloc_addr & ~(W25Q_SECTOR_SIZE - 1);
            sector_end   = end_addr | (W25Q_SECTOR_SIZE - 1);
        }
        retry++;
    }

    /* 擦除目标扇区 */
    for (erase_addr = sector_start; erase_addr <= sector_end; erase_addr += W25Q_SECTOR_SIZE)
    {
        if (BSP_Flash_SectorErase(erase_addr) != HAL_OK)
        {
            return 0;
        }
    }

    /* 更新超级块（仅内存，不持久化——由调用者 FS_File_Write 在索引提交后调用 FS_FlushSuperBlock） */
    s_superblock.next_write_addr = end_addr + 1;
    if (s_superblock.next_write_addr > FS_ADDR_DATA_END)
    {
        s_superblock.next_write_addr = FS_ADDR_DATA_START;
    }

    /* total_data_size: 扣除被无效化的旧 LOG 数据，加上新分配 */
    new_total = s_superblock.total_data_size;
    new_total = (new_total > freed_bytes) ? (new_total - freed_bytes) : 0;
    new_total += size;
    if (new_total > FS_DATA_SIZE)
    {
        new_total = FS_DATA_SIZE;
    }
    s_superblock.total_data_size = new_total;

    return alloc_addr;
}

/**
 * @brief 持久化超级块到Flash
 */
FSResult_e FS_FlushSuperBlock(void)
{
    uint8_t write_buf[sizeof(SuperBlock_t)];
    uint8_t read_buf[sizeof(SuperBlock_t)];

    /* 计算并更新CRC */
    s_superblock.crc = FS_CalcSuperBlockCRC(&s_superblock);

    /* 擦除超级块扇区 */
    if (BSP_Flash_SectorErase(FS_ADDR_SUPER_BLOCK) != HAL_OK)
    {
        return FS_ERR_FLASH;
    }

    /* 写入超级块 */
    memcpy(write_buf, &s_superblock, sizeof(SuperBlock_t));
    if (BSP_Flash_PageWrite(FS_ADDR_SUPER_BLOCK, write_buf, sizeof(SuperBlock_t)) != HAL_OK)
    {
        return FS_ERR_FLASH;
    }

    /* 读回验证 */
    if (BSP_Flash_ReadData(FS_ADDR_SUPER_BLOCK, read_buf, sizeof(SuperBlock_t)) != HAL_OK)
    {
        return FS_ERR_FLASH;
    }

    if (memcmp(write_buf, read_buf, sizeof(SuperBlock_t)) != 0)
    {
        return FS_ERR_FLASH;
    }

    return FS_OK;
}

/**
 * @brief 检查文件系统是否已初始化
 */
uint8_t FS_IsInitialized(void)
{
    return s_initialized;
}
