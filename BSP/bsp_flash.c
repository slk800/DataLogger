/**
 * @file    bsp_flash.c
 * @brief   W25Q64 Flash驱动实现 - 读写擦除操作
 * @author  [Your Name]
 * @date    2025-01-01
 * @version 1.0
 */

#include "bsp_flash.h"
#include "bsp_spi.h"

#if 0

/* ========== 私有变量 ========== */
static uint8_t flash_tx_buf[264];    /* 发送缓冲区 */
static uint8_t flash_rx_buf[264];    /* 接收缓冲区 */
static SemaphoreHandle_t s_FlashMutex = NULL;
#endif
static SemaphoreHandle_t s_FlashMutex = NULL;

/* ========== Flash驱动函数实现 ========== */

/**
 * @brief 初始化Flash驱动
 * @return HAL_OK: 初始化成功, HAL_ERROR: JEDEC ID不匹配
 */
HAL_StatusTypeDef BSP_Flash_Init(void)
{
    uint32_t jedec_id;

    /* 初始化SPI驱动 */
    BSP_SPI_Init();

    /* 读取JEDEC ID验证芯片 */
    jedec_id = BSP_Flash_ReadJEDECID();

    /* 验证是否为W25Q64 (JEDEC ID = 0xEF4017) */
    if (jedec_id != W25Q_JEDEC_ID)
    {
        return HAL_ERROR;
    }

    s_FlashMutex = xSemaphoreCreateMutex();

    return HAL_OK;
}

/**
 * @brief 读取Flash JEDEC ID
 * @return JEDEC ID (厂商ID + 设备ID)
 */
uint32_t BSP_Flash_ReadJEDECID(void)
{
    uint32_t jedec_id = 0;

    /* 拉低片选，选中Flash */
    BSP_SPI_CS_Low();

    /* 发送读JEDEC ID指令 */
    BSP_SPI_TransferByte(W25Q_CMD_READ_JEDEC_ID);

    /* 读取3字节ID (厂商ID + 设备ID) */
    jedec_id  = (uint32_t)BSP_SPI_TransferByte(0xFF) << 16;
    jedec_id |= (uint32_t)BSP_SPI_TransferByte(0xFF) << 8;
    jedec_id |= (uint32_t)BSP_SPI_TransferByte(0xFF);

    /* 拉高片选，释放Flash */
    BSP_SPI_CS_High();

    return jedec_id;
}

/**
 * @brief 等待Flash空闲
 * @note  轮询状态寄存器BUSY位，直到为0
 */
void BSP_Flash_WaitBusy(void)
{
    uint8_t status;
    uint32_t timeout = 0x000FFFFF;    /* 超时计数器 */

    /* 拉低片选 */
    BSP_SPI_CS_Low();

    /* 发送读状态寄存器指令 */
    BSP_SPI_TransferByte(W25Q_CMD_READ_STATUS_REG);

    /* 轮询BUSY位 */
    do
    {
        status = BSP_SPI_TransferByte(0xFF);
        timeout--;

        /* 超时保护，防止死循环 */
        if (timeout == 0)
        {
            break;
        }
    }
    while ((status & W25Q_STATUS_BUSY) != 0);

    /* 拉高片选 */
    BSP_SPI_CS_High();
}

/**
 * @brief Flash写使能
 * @note  每次页编程或擦除前需要调用
 */
void BSP_Flash_WriteEnable(void)
{
    /* 拉低片选 */
    BSP_SPI_CS_Low();

    /* 发送写使能指令 */
    BSP_SPI_TransferByte(W25Q_CMD_WRITE_ENABLE);

    /* 拉高片选 */
    BSP_SPI_CS_High();
}

/**
 * @brief 从Flash读取数据
 * @param addr 起始地址
 * @param buf  数据缓冲区
 * @param len  读取长度
 * @return HAL状态
 */
HAL_StatusTypeDef BSP_Flash_ReadData(uint32_t addr, uint8_t *buf, uint32_t len)
{
    uint32_t i;

    /* 等待Flash空闲 */
    BSP_Flash_WaitBusy();

    /* 拉低片选 */
    BSP_SPI_CS_Low();

    /* 发送读数据指令 */
    BSP_SPI_TransferByte(W25Q_CMD_READ_DATA);

    /* 发送24位地址 (高字节在前) */
    BSP_SPI_TransferByte((uint8_t)(addr >> 16));
    BSP_SPI_TransferByte((uint8_t)(addr >> 8));
    BSP_SPI_TransferByte((uint8_t)(addr));

    /* 读取数据 */
    for (i = 0; i < len; i++)
    {
        buf[i] = BSP_SPI_TransferByte(0xFF);
    }

    /* 拉高片选 */
    BSP_SPI_CS_High();

    return HAL_OK;
}

/**
 * @brief Flash页编程
 * @param addr 起始地址 (建议页对齐，即256字节对齐)
 * @param buf  数据缓冲区
 * @param len  写入长度 (不超过256字节)
 * @return HAL状态
 * @note  页编程只能将1改为0，擦除才能将0改为1
 */
HAL_StatusTypeDef BSP_Flash_PageWrite(uint32_t addr, uint8_t *buf, uint16_t len)
{
    uint16_t i;

    /* 参数检查：长度不超过页大小 */
    if (len > W25Q_PAGE_SIZE)
    {
        return HAL_ERROR;
    }

    /* 等待Flash空闲 */
    BSP_Flash_WaitBusy();

    /* 写使能 */
    BSP_Flash_WriteEnable();

    /* 拉低片选 */
    BSP_SPI_CS_Low();

    /* 发送页编程指令 */
    BSP_SPI_TransferByte(W25Q_CMD_PAGE_PROGRAM);

    /* 发送24位地址 */
    BSP_SPI_TransferByte((uint8_t)(addr >> 16));
    BSP_SPI_TransferByte((uint8_t)(addr >> 8));
    BSP_SPI_TransferByte((uint8_t)(addr));

    /* 发送数据 */
    for (i = 0; i < len; i++)
    {
        BSP_SPI_TransferByte(buf[i]);
    }

    /* 拉高片选 */
    BSP_SPI_CS_High();

    /* 等待编程完成 */
    BSP_Flash_WaitBusy();

    return HAL_OK;
}

/**
 * @brief 擦除4KB扇区
 * @param addr 扇区内任意地址 (内部自动4KB对齐)
 * @return HAL状态
 */
HAL_StatusTypeDef BSP_Flash_SectorErase(uint32_t addr)
{
    /* 地址4KB对齐 */
    addr &= ~(W25Q_SECTOR_SIZE - 1);

    /* 等待Flash空闲 */
    BSP_Flash_WaitBusy();

    /* 写使能 */
    BSP_Flash_WriteEnable();

    /* 拉低片选 */
    BSP_SPI_CS_Low();

    /* 发送扇区擦除指令 */
    BSP_SPI_TransferByte(W25Q_CMD_SECTOR_ERASE);

    /* 发送24位地址 */
    BSP_SPI_TransferByte((uint8_t)(addr >> 16));
    BSP_SPI_TransferByte((uint8_t)(addr >> 8));
    BSP_SPI_TransferByte((uint8_t)(addr));

    /* 拉高片选 */
    BSP_SPI_CS_High();

    /* 等待擦除完成 */
    BSP_Flash_WaitBusy();

    return HAL_OK;
}

/**
 * @brief 擦除64KB块
 * @param addr 块内任意地址
 * @return HAL状态
 */
HAL_StatusTypeDef BSP_Flash_BlockErase(uint32_t addr)
{
    /* 等待Flash空闲 */
    BSP_Flash_WaitBusy();

    /* 写使能 */
    BSP_Flash_WriteEnable();

    /* 拉低片选 */
    BSP_SPI_CS_Low();

    /* 发送64KB块擦除指令 */
    BSP_SPI_TransferByte(W25Q_CMD_BLOCK_ERASE_64K);

    /* 发送24位地址 */
    BSP_SPI_TransferByte((uint8_t)(addr >> 16));
    BSP_SPI_TransferByte((uint8_t)(addr >> 8));
    BSP_SPI_TransferByte((uint8_t)(addr));

    /* 拉高片选 */
    BSP_SPI_CS_High();

    /* 等待擦除完成 */
    BSP_Flash_WaitBusy();

    return HAL_OK;
}

/**
 * @brief 全片擦除
 * @return HAL状态
 * @note  擦除时间较长，约需数秒
 */
HAL_StatusTypeDef BSP_Flash_ChipErase(void)
{
    /* 等待Flash空闲 */
    BSP_Flash_WaitBusy();

    /* 写使能 */
    BSP_Flash_WriteEnable();

    /* 拉低片选 */
    BSP_SPI_CS_Low();

    /* 发送全片擦除指令 */
    BSP_SPI_TransferByte(W25Q_CMD_CHIP_ERASE);

    /* 拉高片选 */
    BSP_SPI_CS_High();

    /* 等待擦除完成 (全片擦除需要较长时间) */
    BSP_Flash_WaitBusy();

    return HAL_OK;
}

/* ========== Flash互斥锁实现 ========== */

bool BSP_Flash_Lock(uint32_t timeout_ms)
{
    if (s_FlashMutex == NULL)
    {
        return false;
    }
    return (xSemaphoreTake(s_FlashMutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE);
}

void BSP_Flash_Unlock(void)
{
    if (s_FlashMutex != NULL)
    {
        xSemaphoreGive(s_FlashMutex);
    }
}
