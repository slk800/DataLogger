/**
 * @file    bsp_flash.c
 * @brief   W25Q64 Flash驱动实现 - 读写擦除操作
 * @author  SLK800
 * @date    2026-05
 * @version 1.1
 *
 * @note
 * 本驱动会透传 HAL_SPI 的返回状态：
 *   - HAL_OK      命令与数据已完整发出并完成
 *   - HAL_ERROR   SPI 传输失败
 *   - HAL_TIMEOUT 轮询 BUSY 位超时
 * 上层（FS / Config）据此判断事务是否真正成功，
 * 不再出现"驱动永远返回 OK"的假阳性。
 */

#include "bsp_flash.h"
#include "bsp_spi.h"

static SemaphoreHandle_t s_FlashMutex = NULL;

/* ========== 私有辅助函数 ========== */

/**
 * @brief 等待Flash空闲（带状态返回）
 * @return HAL_OK / HAL_ERROR(SPI失败) / HAL_TIMEOUT(BUSY超时)
 */
static HAL_StatusTypeDef BSP_Flash_WaitBusyEx(void)
{
    uint8_t status = 0xFFU;
    uint32_t timeout = 0x000FFFFFUL;    /* 超时计数器 */
    HAL_StatusTypeDef ret;

    BSP_SPI_CS_Low();

    ret = BSP_SPI_TransferByteStatus(W25Q_CMD_READ_STATUS_REG, NULL);
    if (ret != HAL_OK)
    {
        BSP_SPI_CS_High();
        return ret;
    }

    do
    {
        ret = BSP_SPI_TransferByteStatus(0xFFU, &status);
        if (ret != HAL_OK)
        {
            BSP_SPI_CS_High();
            return ret;
        }
        timeout--;
    }
    while (((status & W25Q_STATUS_BUSY) != 0U) && (timeout != 0U));

    BSP_SPI_CS_High();

    return (timeout == 0U) ? HAL_TIMEOUT : HAL_OK;
}

/**
 * @brief Flash写使能（带状态返回）
 */
static HAL_StatusTypeDef BSP_Flash_WriteEnableEx(void)
{
    HAL_StatusTypeDef ret;

    BSP_SPI_CS_Low();
    ret = BSP_SPI_TransferByteStatus(W25Q_CMD_WRITE_ENABLE, NULL);
    BSP_SPI_CS_High();

    return ret;
}

/**
 * @brief 发送"命令 + 24位地址"并保持片选（调用者负责 CS）
 */
static HAL_StatusTypeDef BSP_Flash_SendCmdAddr(uint8_t cmd, uint32_t addr)
{
    HAL_StatusTypeDef ret;

    ret = BSP_SPI_TransferByteStatus(cmd, NULL);
    if (ret != HAL_OK)
    {
        return ret;
    }
    ret = BSP_SPI_TransferByteStatus((uint8_t)(addr >> 16), NULL);
    if (ret != HAL_OK)
    {
        return ret;
    }
    ret = BSP_SPI_TransferByteStatus((uint8_t)(addr >> 8), NULL);
    if (ret != HAL_OK)
    {
        return ret;
    }
    return BSP_SPI_TransferByteStatus((uint8_t)(addr), NULL);
}

/* ========== 公共函数实现 ========== */

/**
 * @brief 初始化Flash驱动
 * @return HAL_OK: 初始化成功, HAL_ERROR: JEDEC ID不匹配或SPI失败
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
    if (s_FlashMutex == NULL)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

/**
 * @brief 读取Flash JEDEC ID
 * @return JEDEC ID (厂商ID + 设备ID)；SPI失败时返回0
 */
uint32_t BSP_Flash_ReadJEDECID(void)
{
    uint32_t jedec_id = 0U;
    uint8_t b1 = 0U;
    uint8_t b2 = 0U;
    uint8_t b3 = 0U;

    BSP_SPI_CS_Low();

    if (BSP_SPI_TransferByteStatus(W25Q_CMD_READ_JEDEC_ID, NULL) != HAL_OK ||
        BSP_SPI_TransferByteStatus(0xFFU, &b1) != HAL_OK ||
        BSP_SPI_TransferByteStatus(0xFFU, &b2) != HAL_OK ||
        BSP_SPI_TransferByteStatus(0xFFU, &b3) != HAL_OK)
    {
        BSP_SPI_CS_High();
        return 0U;
    }

    BSP_SPI_CS_High();

    jedec_id  = ((uint32_t)b1 << 16);
    jedec_id |= ((uint32_t)b2 << 8);
    jedec_id |= (uint32_t)b3;

    return jedec_id;
}

/**
 * @brief 等待Flash空闲
 * @return HAL状态；超时返回 HAL_TIMEOUT
 * @note  擦除与页编程内部已包含等待，通常无需再调用。
 */
HAL_StatusTypeDef BSP_Flash_WaitBusy(void)
{
    return BSP_Flash_WaitBusyEx();
}

/**
 * @brief Flash写使能
 * @return HAL状态
 * @note  页编程与擦除内部已包含写使能，通常无需再调用。
 */
HAL_StatusTypeDef BSP_Flash_WriteEnable(void)
{
    return BSP_Flash_WriteEnableEx();
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
    HAL_StatusTypeDef ret;

    if ((buf == NULL) || (len == 0U) || ((addr + len) > W25Q_FLASH_TOTAL_SIZE))
    {
        return HAL_ERROR;
    }

    ret = BSP_Flash_WaitBusyEx();
    if (ret != HAL_OK)
    {
        return ret;
    }

    BSP_SPI_CS_Low();

    ret = BSP_Flash_SendCmdAddr(W25Q_CMD_READ_DATA, addr);
    if (ret != HAL_OK)
    {
        BSP_SPI_CS_High();
        return ret;
    }

    for (i = 0U; i < len; i++)
    {
        ret = BSP_SPI_TransferByteStatus(0xFFU, &buf[i]);
        if (ret != HAL_OK)
        {
            BSP_SPI_CS_High();
            return ret;
        }
    }

    BSP_SPI_CS_High();

    return HAL_OK;
}

/**
 * @brief Flash页编程
 * @param addr 起始地址 (256字节页内)
 * @param buf  数据缓冲区
 * @param len  写入长度 (1~256，且不得跨页)
 * @return HAL状态
 * @note  页编程只能将1改为0，擦除才能将0改为1。
 *        跨页写入会被W25Q64回卷到页首，因此这里直接拒绝。
 */
HAL_StatusTypeDef BSP_Flash_PageWrite(uint32_t addr, uint8_t *buf, uint16_t len)
{
    uint16_t i;
    HAL_StatusTypeDef ret;

    if ((buf == NULL) || (len == 0U) || (len > W25Q_PAGE_SIZE))
    {
        return HAL_ERROR;
    }

    if ((((uint32_t)addr % W25Q_PAGE_SIZE) + (uint32_t)len) > W25Q_PAGE_SIZE)
    {
        return HAL_ERROR;
    }

    if ((addr + (uint32_t)len) > W25Q_FLASH_TOTAL_SIZE)
    {
        return HAL_ERROR;
    }

    ret = BSP_Flash_WaitBusyEx();
    if (ret != HAL_OK)
    {
        return ret;
    }

    ret = BSP_Flash_WriteEnableEx();
    if (ret != HAL_OK)
    {
        return ret;
    }

    BSP_SPI_CS_Low();

    ret = BSP_Flash_SendCmdAddr(W25Q_CMD_PAGE_PROGRAM, addr);
    if (ret != HAL_OK)
    {
        BSP_SPI_CS_High();
        return ret;
    }

    for (i = 0U; i < len; i++)
    {
        ret = BSP_SPI_TransferByteStatus(buf[i], NULL);
        if (ret != HAL_OK)
        {
            BSP_SPI_CS_High();
            return ret;
        }
    }

    BSP_SPI_CS_High();

    return BSP_Flash_WaitBusyEx();
}

/**
 * @brief 擦除4KB扇区
 * @param addr 扇区内任意地址 (内部自动4KB对齐)
 * @return HAL状态
 */
HAL_StatusTypeDef BSP_Flash_SectorErase(uint32_t addr)
{
    HAL_StatusTypeDef ret;

    if (addr >= W25Q_FLASH_TOTAL_SIZE)
    {
        return HAL_ERROR;
    }

    /* 地址4KB对齐 */
    addr &= ~(W25Q_SECTOR_SIZE - 1U);

    ret = BSP_Flash_WaitBusyEx();
    if (ret != HAL_OK)
    {
        return ret;
    }

    ret = BSP_Flash_WriteEnableEx();
    if (ret != HAL_OK)
    {
        return ret;
    }

    BSP_SPI_CS_Low();
    ret = BSP_Flash_SendCmdAddr(W25Q_CMD_SECTOR_ERASE, addr);
    BSP_SPI_CS_High();

    if (ret != HAL_OK)
    {
        return ret;
    }

    return BSP_Flash_WaitBusyEx();
}

/**
 * @brief 擦除64KB块
 * @param addr 块内任意地址
 * @return HAL状态
 */
HAL_StatusTypeDef BSP_Flash_BlockErase(uint32_t addr)
{
    HAL_StatusTypeDef ret;

    if (addr >= W25Q_FLASH_TOTAL_SIZE)
    {
        return HAL_ERROR;
    }

    ret = BSP_Flash_WaitBusyEx();
    if (ret != HAL_OK)
    {
        return ret;
    }

    ret = BSP_Flash_WriteEnableEx();
    if (ret != HAL_OK)
    {
        return ret;
    }

    BSP_SPI_CS_Low();
    ret = BSP_Flash_SendCmdAddr(W25Q_CMD_BLOCK_ERASE_64K, addr);
    BSP_SPI_CS_High();

    if (ret != HAL_OK)
    {
        return ret;
    }

    return BSP_Flash_WaitBusyEx();
}

/**
 * @brief 全片擦除
 * @return HAL状态
 * @note  擦除时间较长，约需数秒
 */
HAL_StatusTypeDef BSP_Flash_ChipErase(void)
{
    HAL_StatusTypeDef ret;

    ret = BSP_Flash_WaitBusyEx();
    if (ret != HAL_OK)
    {
        return ret;
    }

    ret = BSP_Flash_WriteEnableEx();
    if (ret != HAL_OK)
    {
        return ret;
    }

    BSP_SPI_CS_Low();
    ret = BSP_SPI_TransferByteStatus(W25Q_CMD_CHIP_ERASE, NULL);
    BSP_SPI_CS_High();

    if (ret != HAL_OK)
    {
        return ret;
    }

    return BSP_Flash_WaitBusyEx();
}

/**
 * @brief 进入 Deep Power Down
 * @note  指令 0xB9。深睡电流约 1 µA，比待机态（~4 mA）低三个数量级。
 *        退出后需要 3 µs 才能接受新命令（见 BSP_Flash_Wake）。
 */
HAL_StatusTypeDef BSP_Flash_DeepSleep(void)
{
    HAL_StatusTypeDef ret;

    ret = BSP_Flash_WaitBusyEx();
    if (ret != HAL_OK)
    {
        return ret;
    }

    BSP_SPI_CS_Low();
    ret = BSP_SPI_TransferByteStatus(W25Q_CMD_DEEP_POWER_DOWN, NULL);
    BSP_SPI_CS_High();

    return ret;
}

/**
 * @brief 从 Deep Power Down 唤醒
 * @note  指令 0xAB。器件需要 tRES1（3 µs）才能接受新命令，
 *        这里等 1 ms 以留足余量（HAL_Delay 依赖 SysTick，
 *        因此调用方必须保证中断开启）。
 */
HAL_StatusTypeDef BSP_Flash_Wake(void)
{
    HAL_StatusTypeDef ret;

    BSP_SPI_CS_Low();
    ret = BSP_SPI_TransferByteStatus(W25Q_CMD_RELEASE_POWER_DOWN, NULL);
    BSP_SPI_CS_High();

    if (ret == HAL_OK)
    {
        HAL_Delay(1U);
    }

    return ret;
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
