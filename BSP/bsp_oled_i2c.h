/**
 * @file    bsp_oled_i2c.h
 * @brief   OLED I2C board support wrapper.
 */

#ifndef __BSP_OLED_I2C_H
#define __BSP_OLED_I2C_H

#include "stm32f1xx_hal.h"
#include "i2c.h"
#include <stdint.h>

#define OLED_I2C_ADDR_DEFAULT  (0x3C << 1)
#define OLED_I2C_TIMEOUT_MS    50U

HAL_StatusTypeDef BSP_OLED_I2C_IsReady(uint16_t dev_addr);
HAL_StatusTypeDef BSP_OLED_I2C_Write(uint16_t dev_addr, const uint8_t *data, uint16_t len);

#endif /* __BSP_OLED_I2C_H */
