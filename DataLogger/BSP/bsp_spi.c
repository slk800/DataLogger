/**
 * @file    bsp_spi.c
 * @brief   SPI底层驱动实现 - 片选控制与数据收发
 * @author  [Your Name]
 * @date    2025-01-01
 * @version 1.0
 */

#include "bsp_spi.h"
#include "spi.h"    /* CubeMX生成的hspi1句柄声明 */

/* ========== SPI驱动函数实现 ========== */

/**
 * @brief 初始化SPI驱动
 * @note  硬件初始化由CubeMX完成，此处仅验证句柄可用
 */
void BSP_SPI_Init(void)
{
    /* 验证SPI句柄是否已初始化 */
    if (hspi1.Instance != SPI1)
    {
        /* SPI句柄未正确初始化，此处可添加错误处理 */
        return;
    }
}

/**
 * @brief 拉低片选信号
 * @note  选中W25Q64 Flash芯片
 */
void BSP_SPI_CS_Low(void)
{
    HAL_GPIO_WritePin(W25Q_CS_GPIO_Port, W25Q_CS_Pin, GPIO_PIN_RESET);
}

/**
 * @brief 拉高片选信号
 * @note  释放W25Q64 Flash芯片
 */
void BSP_SPI_CS_High(void)
{
    HAL_GPIO_WritePin(W25Q_CS_GPIO_Port, W25Q_CS_Pin, GPIO_PIN_SET);
}

/**
 * @brief SPI收发单字节
 * @param data 待发送的字节
 * @return 接收到的字节
 */
uint8_t BSP_SPI_TransferByte(uint8_t data)
{
    uint8_t rx_data = 0xFF;

    /* 全双工收发，超时100ms */
    HAL_SPI_TransmitReceive(&hspi1, &data, &rx_data, 1, 100);

    return rx_data;
}

/**
 * @brief SPI收发多字节
 * @param tx_buf 发送缓冲区指针
 * @param rx_buf 接收缓冲区指针
 * @param len    数据长度
 * @return HAL状态
 */
HAL_StatusTypeDef BSP_SPI_TransferBuf(uint8_t *tx_buf, uint8_t *rx_buf, uint16_t len)
{
    /* 全双工收发多字节 */
    return HAL_SPI_TransmitReceive(&hspi1, tx_buf, rx_buf, len, 1000);
}
