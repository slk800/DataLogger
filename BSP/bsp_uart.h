/**
 * @file    bsp_uart.h
 * @brief   串口底层驱动头文件 - 环形缓冲区定义与串口函数声明
 * @author  [Your Name]
 * @date    2025-01-01
 * @version 1.0
 */

#ifndef __BSP_UART_H
#define __BSP_UART_H

#include "global.h"

/* ========== 宏定义 ========== */
#define RING_BUF_SIZE    256    /* 环形缓冲区大小 */

/* ========== 类型定义 ========== */

/**
 * @brief 环形缓冲区结构体
 */
typedef struct
{
    uint8_t  buffer[RING_BUF_SIZE];    /* 数据存储区 */
    uint16_t head;                     /* 读指针 */
    uint16_t tail;                     /* 写指针 */
    uint16_t size;                     /* 缓冲区大小 */
} RingBuffer_t;

/* ========== 环形缓冲区函数声明 ========== */
void     RB_Init(RingBuffer_t *rb);
uint16_t RB_Write(RingBuffer_t *rb, uint8_t *data, uint16_t len);
uint16_t RB_Read(RingBuffer_t *rb, uint8_t *data, uint16_t len);
uint8_t  RB_IsEmpty(RingBuffer_t *rb);
uint16_t RB_Available(RingBuffer_t *rb);

/* ========== 串口驱动函数声明 ========== */
void    BSP_UART_Init(void);
void    BSP_UART_SendByte(uint8_t data);
void    BSP_UART_SendString(const char *str);
void    BSP_UART_SendBuf(uint8_t *buf, uint16_t len);

/* ========== 全局变量声明 ========== */
extern RingBuffer_t uart_rx_rb;    /* 串口接收环形缓冲区 */

#endif /* __BSP_UART_H */
