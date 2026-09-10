/**
 * @file    global.h
 * @brief   全局定义头文件 - 引脚定义、全局宏、模块引用汇总
 * @author  SLK800
 * @date    2026-05-01
 * @version 1.0
 */

#ifndef __GLOBAL_H
#define __GLOBAL_H

/* ========== 标准库头文件 ========== */
#include "stm32f1xx_hal.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

/* ========== 引脚定义区 ========== */

/* SPI1 Flash (W25Q64) */
#define SPI1_SCK_PIN        GPIO_PIN_5
#define SPI1_SCK_PORT       GPIOA
#define SPI1_MISO_PIN       GPIO_PIN_6
#define SPI1_MISO_PORT      GPIOA
#define SPI1_MOSI_PIN       GPIO_PIN_7
#define SPI1_MOSI_PORT      GPIOA
#define W25Q_CS_PIN         GPIO_PIN_0
#define W25Q_CS_PORT        GPIOB

/* USART1 */
#define USART1_TX_PIN       GPIO_PIN_9
#define USART1_TX_PORT      GPIOA
#define USART1_RX_PIN       GPIO_PIN_10
#define USART1_RX_PORT      GPIOA

/* LED */
#define LED_PIN             GPIO_PIN_13
#define LED_PORT            GPIOC

/* ========== 全局宏定义区 ========== */

#define FIRMWARE_VERSION    "1.0.0"
#define PROJECT_NAME        "DataLogger"
#define MCU_MODEL           "STM32F103C8T6"
#define FLASH_SIZE_BYTES    (8 * 1024 * 1024)   /* W25Q64 8MB */
#define SRAM_SIZE_BYTES     (20 * 1024)         /* STM32F103C8 20KB */
#define MCU_CLOCK_HZ        72000000UL

/* ========== 全局类型定义区 ========== */

/* ========== 全局变量声明区（extern）========== */

/* ========== 全局函数声明区 ========== */

/* ========== 模块头文件引用集合（其他.c只需 #include "global.h"）========== */

/* BSP 层 */
#include "bsp_uart.h"
#include "bsp_flash.h"
#include "bsp_spi.h"
#include "bsp_oled_i2c.h"
#include "bsp_key.h"

/* OLED */
#include "ssd1306.h"

/* Utils */
#include "utils.h"

/* Sensor */
#include "sensor_ntc.h"

/* Log */
#include "sample_record.h"

/* FS 层 */
#include "fs_types.h"
#include "fs_core.h"
#include "fs_file.h"

/* 应用层 */
#include "ui_types.h"
#include "ui_status.h"
#include "ui_menu.h"
#include "log.h"
#include "cli.h"
#include "cli_cmd.h"
#include "config.h"
#include "app_init.h"
#include "task_logger.h"
#include "task_shell.h"
#include "task_data_producer.h"
#include "task_display.h"
#include "task_monitor.h"

#endif /* __GLOBAL_H */
