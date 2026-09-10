/**
 * @file    bsp_flash.h
 * @brief   W25Q64 Flash驱动头文件 - 指令定义与函数声明
 * @author  [Your Name]
 * @date    2025-01-01
 * @version 1.0
 */

#ifndef __BSP_FLASH_H
#define __BSP_FLASH_H

#include "global.h"
#include <stdbool.h>

/* ========== W25Q64 指令定义 ========== */
#define W25Q_CMD_READ_DATA          0x03    /* 读数据 */
#define W25Q_CMD_FAST_READ          0x0B    /* 快速读 */
#define W25Q_CMD_PAGE_PROGRAM       0x02    /* 页编程 */
#define W25Q_CMD_SECTOR_ERASE       0x20    /* 4KB扇区擦除 */
#define W25Q_CMD_BLOCK_ERASE_32K    0x52    /* 32KB块擦除 */
#define W25Q_CMD_BLOCK_ERASE_64K    0xD8    /* 64KB块擦除 */
#define W25Q_CMD_CHIP_ERASE         0xC7    /* 全片擦除 */
#define W25Q_CMD_WRITE_ENABLE       0x06    /* 写使能 */
#define W25Q_CMD_WRITE_DISABLE      0x04    /* 写禁止 */
#define W25Q_CMD_READ_STATUS_REG    0x05    /* 读状态寄存器 */
#define W25Q_CMD_WRITE_STATUS_REG   0x01    /* 写状态寄存器 */
#define W25Q_CMD_READ_JEDEC_ID      0x9F    /* 读JEDEC ID */

/* ========== W25Q64 状态寄存器位定义 ========== */
#define W25Q_STATUS_BUSY            0x01    /* 忙标志位 */
#define W25Q_STATUS_WEL             0x02    /* 写使能锁存位 */

/* ========== W25Q64 容量参数 ========== */
#define W25Q_PAGE_SIZE              256         /* 页大小: 256字节 */
#define W25Q_SECTOR_SIZE            4096        /* 扇区大小: 4KB */
#define W25Q_BLOCK_SIZE             65536       /* 块大小: 64KB */
#define W25Q_FLASH_TOTAL_SIZE       0x800000    /* 总容量: 8MB */

/* ========== JEDEC ID ========== */
#define W25Q_JEDEC_ID               0xEF4017    /* W25Q64 JEDEC ID */

/* ========== Flash驱动函数声明 ========== */
HAL_StatusTypeDef BSP_Flash_Init(void);
uint32_t          BSP_Flash_ReadJEDECID(void);
void              BSP_Flash_WaitBusy(void);
void              BSP_Flash_WriteEnable(void);
HAL_StatusTypeDef BSP_Flash_ReadData(uint32_t addr, uint8_t *buf, uint32_t len);
HAL_StatusTypeDef BSP_Flash_PageWrite(uint32_t addr, uint8_t *buf, uint16_t len);
HAL_StatusTypeDef BSP_Flash_SectorErase(uint32_t addr);
HAL_StatusTypeDef BSP_Flash_BlockErase(uint32_t addr);
HAL_StatusTypeDef BSP_Flash_ChipErase(void);

bool BSP_Flash_Lock(uint32_t timeout_ms);
void BSP_Flash_Unlock(void);

#endif /* __BSP_FLASH_H */
