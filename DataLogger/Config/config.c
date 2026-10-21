/**
 * @file    config.c
 * @brief   系统配置模块实现 - 加载/保存/读写，双区冗余
 * @author  SLK800
 * @date    2026-05-01
 * @version 1.0
 */

#include "config.h"
#include "app_init.h"
#include "log.h"
#include "bsp_uart.h"
#include "bsp_flash.h"
#include "utils.h"
#include <string.h>
#include <stdio.h>

/* ========== 全局变量 ========== */
SysConfig_t g_config;

/* ========== 内部辅助函数 ========== */

/**
 * @brief 计算配置结构体CRC32
 * @param cfg 配置指针
 * @return CRC32值（不含crc字段本身）
 */
static uint32_t Config_CalcCRC(const SysConfig_t *cfg)
{
    size_t len = sizeof(SysConfig_t) - sizeof(uint32_t);
    return Utils_CRC32_Calc((const uint8_t *)cfg, (uint32_t)len);
}

/* ========== API实现 ========== */

void Config_LoadDefault(void)
{
    memset(&g_config, 0, sizeof(g_config));
    g_config.magic               = CONFIG_MAGIC;
    g_config.version             = 1;
    g_config.sample_interval_ms  = 1000;
    g_config.log_level           = (uint32_t)LOG_LEVEL_INFO;
    g_config.alarm_low_x100      = 200;     /* 2.00 °C */
    g_config.alarm_high_x100     = 800;     /* 8.00 °C */
    g_config.alarm_hyst_x100     = 50;      /* 0.50 °C */
    g_config.alarm_min_duration_s = 60U;
    strncpy(g_config.device_name, "DataLogger", sizeof(g_config.device_name) - 1);
    g_config.device_name[sizeof(g_config.device_name) - 1] = '\0';
}

HAL_StatusTypeDef Config_Load(void)
{
    SysConfig_t cfgA, cfgB;
    uint8_t a_valid = 0, b_valid = 0;

    /* 读取A区并验证 */
    if (BSP_Flash_ReadData(CONFIG_ADDR_A, (uint8_t *)&cfgA, sizeof(SysConfig_t)) == HAL_OK)
    {
        if (cfgA.magic == CONFIG_MAGIC && cfgA.crc == Config_CalcCRC(&cfgA))
        {
            a_valid = 1;
        }
    }

    /* 读取B区并验证 */
    if (BSP_Flash_ReadData(CONFIG_ADDR_B, (uint8_t *)&cfgB, sizeof(SysConfig_t)) == HAL_OK)
    {
        if (cfgB.magic == CONFIG_MAGIC && cfgB.crc == Config_CalcCRC(&cfgB))
        {
            b_valid = 1;
        }
    }

    if (a_valid && b_valid)
    {
        /* 双区均有效：选版本号高的 */
        SysConfig_t *winner = (cfgB.version > cfgA.version) ? &cfgB : &cfgA;
        memcpy(&g_config, winner, sizeof(SysConfig_t));
        g_log_level = (LogLevel_e)g_config.log_level;
        return HAL_OK;
    }

    if (b_valid)
    {
        /* 仅B区有效：从B恢复，修复A */
        memcpy(&g_config, &cfgB, sizeof(SysConfig_t));
        g_log_level = (LogLevel_e)g_config.log_level;
        (void)BSP_Flash_SectorErase(CONFIG_ADDR_A);
        (void)BSP_Flash_PageWrite(CONFIG_ADDR_A, (uint8_t *)&g_config, sizeof(SysConfig_t));
        return HAL_OK;
    }

    if (a_valid)
    {
        /* 仅A区有效：从A恢复，修复B */
        memcpy(&g_config, &cfgA, sizeof(SysConfig_t));
        g_log_level = (LogLevel_e)g_config.log_level;
        (void)BSP_Flash_SectorErase(CONFIG_ADDR_B);
        (void)BSP_Flash_PageWrite(CONFIG_ADDR_B, (uint8_t *)&g_config, sizeof(SysConfig_t));
        return HAL_OK;
    }

    /* 两区均失败，使用默认值 */
    Config_LoadDefault();
    return HAL_ERROR;
}

HAL_StatusTypeDef Config_Save(void)
{
    SysConfig_t verify;
    HAL_StatusTypeDef ret = HAL_OK;

    /* 递增版本号 */
    g_config.version++;

    /* 确保魔数正确并计算CRC */
    g_config.magic = CONFIG_MAGIC;
    g_config.crc = Config_CalcCRC(&g_config);

    /* 获取Flash互斥锁 */
    if (!BSP_Flash_Lock(1000))
    {
        return HAL_TIMEOUT;
    }

    /* 先写B区（新数据先落B，A仍存旧有效数据） */
    if (BSP_Flash_SectorErase(CONFIG_ADDR_B) != HAL_OK ||
        BSP_Flash_PageWrite(CONFIG_ADDR_B, (uint8_t *)&g_config, sizeof(SysConfig_t)) != HAL_OK)
    {
        BSP_UART_SendString("[CFG] B write failed\r\n");
        BSP_Flash_Unlock();
        return HAL_ERROR;
    }

    /* 验证B区 */
    memset(&verify, 0, sizeof(verify));
    if (BSP_Flash_ReadData(CONFIG_ADDR_B, (uint8_t *)&verify, sizeof(SysConfig_t)) != HAL_OK ||
        verify.magic != CONFIG_MAGIC || verify.crc != Config_CalcCRC(&verify))
    {
        BSP_UART_SendString("[CFG] B verify failed\r\n");
        ret = HAL_ERROR;
    }
    else
    {
        /* B区验证通过：先擦除A区使其失效，再写A区 */
        /* 若擦除A后断电 → A无效、B有效 → Load选B → 正确恢复 */
        if (BSP_Flash_SectorErase(CONFIG_ADDR_A) != HAL_OK ||
            BSP_Flash_PageWrite(CONFIG_ADDR_A, (uint8_t *)&g_config, sizeof(SysConfig_t)) != HAL_OK)
        {
            BSP_UART_SendString("[CFG] A write failed\r\n");
            BSP_Flash_Unlock();
            return HAL_ERROR;
        }

        /* 验证A区（若失败，B区仍是本次提交的完整副本） */
        memset(&verify, 0, sizeof(verify));
        if (BSP_Flash_ReadData(CONFIG_ADDR_A, (uint8_t *)&verify, sizeof(SysConfig_t)) != HAL_OK ||
            verify.magic != CONFIG_MAGIC || verify.crc != Config_CalcCRC(&verify))
        {
            BSP_UART_SendString("[CFG] A verify failed\r\n");
            ret = HAL_ERROR;
        }
    }

    BSP_Flash_Unlock();
    return ret;
}

HAL_StatusTypeDef Config_Set(const char *key, const char *val)
{
    if (strcmp(key, "sample_ms") == 0)
    {
        g_config.sample_interval_ms = (uint32_t)Utils_StrToInt(val);
        return HAL_OK;
    }

    if (strcmp(key, "log_level") == 0)
    {
        int32_t level = Utils_StrToInt(val);
        if (level < 0 || level > 3)
        {
            return HAL_ERROR;
        }
        g_config.log_level = (uint32_t)level;
        g_log_level = (LogLevel_e)level;
        return HAL_OK;
    }

    if (strcmp(key, "name") == 0)
    {
        strncpy(g_config.device_name, val, sizeof(g_config.device_name) - 1);
        g_config.device_name[sizeof(g_config.device_name) - 1] = '\0';
        return HAL_OK;
    }

    /* ---- 报警阈值：单位 0.01 °C，与 domain 层保持一致 ---- */
    if (strcmp(key, "t_low") == 0)
    {
        int32_t v = Utils_StrToInt(val);
        if (v < -4000 || v > 12500) return HAL_ERROR;
        g_config.alarm_low_x100 = (int16_t)v;
        return HAL_OK;
    }

    if (strcmp(key, "t_high") == 0)
    {
        int32_t v = Utils_StrToInt(val);
        if (v < -4000 || v > 12500) return HAL_ERROR;
        g_config.alarm_high_x100 = (int16_t)v;
        return HAL_OK;
    }

    if (strcmp(key, "t_hyst") == 0)
    {
        int32_t v = Utils_StrToInt(val);
        if (v < 0 || v > 2000) return HAL_ERROR;
        g_config.alarm_hyst_x100 = (int16_t)v;
        return HAL_OK;
    }

    if (strcmp(key, "t_dur") == 0)
    {
        int32_t v = Utils_StrToInt(val);
        if (v < 1 || v > 3600) return HAL_ERROR;
        g_config.alarm_min_duration_s = (uint16_t)v;
        return HAL_OK;
    }

    return HAL_ERROR;
}

HAL_StatusTypeDef Config_Get(const char *key, char *buf, uint8_t bufLen)
{
    if (strcmp(key, "sample_ms") == 0)
    {
        snprintf(buf, bufLen, "%lu", g_config.sample_interval_ms);
        return HAL_OK;
    }

    if (strcmp(key, "log_level") == 0)
    {
        snprintf(buf, bufLen, "%lu", g_config.log_level);
        return HAL_OK;
    }

    if (strcmp(key, "name") == 0)
    {
        snprintf(buf, bufLen, "%s", g_config.device_name);
        return HAL_OK;
    }

    if (strcmp(key, "t_low") == 0)
    {
        snprintf(buf, bufLen, "%d", (int)g_config.alarm_low_x100);
        return HAL_OK;
    }

    if (strcmp(key, "t_high") == 0)
    {
        snprintf(buf, bufLen, "%d", (int)g_config.alarm_high_x100);
        return HAL_OK;
    }

    if (strcmp(key, "t_hyst") == 0)
    {
        snprintf(buf, bufLen, "%d", (int)g_config.alarm_hyst_x100);
        return HAL_OK;
    }

    if (strcmp(key, "t_dur") == 0)
    {
        snprintf(buf, bufLen, "%u", (unsigned)g_config.alarm_min_duration_s);
        return HAL_OK;
    }

    return HAL_ERROR;
}

void Config_Init(void)
{
    if (Config_Load() != HAL_OK)
    {
        BSP_UART_SendString("[SYS] Using default config\r\n");
        if (Config_Save() == HAL_OK)
        {
            BSP_UART_SendString("[SYS] Default config saved.\r\n");
        }
        else
        {
            BSP_UART_SendString("[SYS] Config save FAILED!\r\n");
        }
    }
}
