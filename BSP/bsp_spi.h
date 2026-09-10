/**
 * @file    bsp_spi.h
 * @brief   SPI底层驱动头文件 - SPI函数声明
 * @author  [Your Name]
 * @date    2025-01-01
 * @version 1.0
 */

#ifndef __BSP_SPI_H
#define __BSP_SPI_H

#include "global.h"

/* ========== SPI驱动函数声明 ========== */
void          BSP_SPI_Init(void);
void          BSP_SPI_CS_Low(void);
void          BSP_SPI_CS_High(void);
uint8_t       BSP_SPI_TransferByte(uint8_t data);
HAL_StatusTypeDef BSP_SPI_TransferBuf(uint8_t *tx_buf, uint8_t *rx_buf, uint16_t len);

#endif /* __BSP_SPI_H */
