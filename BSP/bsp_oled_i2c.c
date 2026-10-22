/**
 * @file    bsp_oled_i2c.c
 * @brief   OLED I2C board support wrapper.
 */

#include "bsp_oled_i2c.h"

HAL_StatusTypeDef BSP_OLED_I2C_IsReady(uint16_t dev_addr)
{
    return HAL_I2C_IsDeviceReady(&hi2c1, dev_addr, 2, OLED_I2C_TIMEOUT_MS);
}

HAL_StatusTypeDef BSP_OLED_I2C_Write(uint16_t dev_addr, const uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0U)
    {
        return HAL_ERROR;
    }

    return HAL_I2C_Master_Transmit(&hi2c1, dev_addr, (uint8_t *)data, len, OLED_I2C_TIMEOUT_MS);
}
