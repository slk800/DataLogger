/**
 * @file    bsp_uart.c
 * @brief   串口底层驱动实现 - 环形缓冲区与中断接收
 * @author  [Your Name]
 * @date    2025-01-01
 * @version 1.0
 */

#include "bsp_uart.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "semphr.h"

/* ========== 全局变量定义 ========== */
RingBuffer_t uart_rx_rb;
static uint8_t uart_rx_byte;
static SemaphoreHandle_t s_uart_tx_mutex = NULL;

/* ========== 环形缓冲区函数实现 ========== */

/**
 * @brief 初始化环形缓冲区
 * @param rb 环形缓冲区指针
 */
void RB_Init(RingBuffer_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
    rb->size = RING_BUF_SIZE;
    memset(rb->buffer, 0, RING_BUF_SIZE);
}

/**
 * @brief 向环形缓冲区写入数据
 * @param rb   环形缓冲区指针
 * @param data 待写入的数据指针
 * @param len  待写入的数据长度
 * @return 实际写入的字节数
 * @note  缓冲区满时拒绝写入（丢弃新数据），返回已写入字节数
 */
uint16_t RB_Write(RingBuffer_t *rb, uint8_t *data, uint16_t len)
{
    uint16_t i;
    uint16_t next_tail;

    for (i = 0; i < len; i++)
    {
        next_tail = (rb->tail + 1) % rb->size;

        /* 缓冲区已满，停止写入 */
        if (next_tail == rb->head)
        {
            break;
        }

        rb->buffer[rb->tail] = data[i];
        rb->tail = next_tail;
    }

    return i;
}

/**
 * @brief 从环形缓冲区读取数据
 * @param rb   环形缓冲区指针
 * @param data 数据存储缓冲区指针
 * @param len  期望读取的数据长度
 * @return 实际读取的字节数
 */
uint16_t RB_Read(RingBuffer_t *rb, uint8_t *data, uint16_t len)
{
    uint16_t i;

    for (i = 0; i < len; i++)
    {
        /* 缓冲区为空，停止读取 */
        if (rb->head == rb->tail)
        {
            break;
        }

        data[i] = rb->buffer[rb->head];
        rb->head = (rb->head + 1) % rb->size;
    }

    return i;
}

/**
 * @brief 检查环形缓冲区是否为空
 * @param rb 环形缓冲区指针
 * @return 1: 为空, 0: 非空
 */
uint8_t RB_IsEmpty(RingBuffer_t *rb)
{
    return (rb->head == rb->tail) ? 1 : 0;
}

/**
 * @brief 获取环形缓冲区中可读数据长度
 * @param rb 环形缓冲区指针
 * @return 可读数据字节数
 */
uint16_t RB_Available(RingBuffer_t *rb)
{
    if (rb->tail >= rb->head)
    {
        return rb->tail - rb->head;
    }
    else
    {
        return rb->size - rb->head + rb->tail;
    }
}

/* ========== 串口驱动函数实现 ========== */

/**
 * @brief 初始化串口驱动
 * @note  硬件初始化由CubeMX完成，此处仅初始化缓冲区并开启中断接收
 */
void BSP_UART_Init(void)
{
    RB_Init(&uart_rx_rb);

    if (s_uart_tx_mutex == NULL)
    {
        s_uart_tx_mutex = xSemaphoreCreateMutex();
    }

    HAL_UART_Receive_IT(&huart1, &uart_rx_byte, 1);
}

/**
 * @brief 发送单字节数据
 * @param data 待发送的字节
 */
void BSP_UART_SendByte(uint8_t data)
{
    if (s_uart_tx_mutex)
    {
        xSemaphoreTake(s_uart_tx_mutex, pdMS_TO_TICKS(1000));
    }
    HAL_UART_Transmit(&huart1, &data, 1, 100);
    if (s_uart_tx_mutex)
    {
        xSemaphoreGive(s_uart_tx_mutex);
    }
}

/**
 * @brief 发送字符串（整个字符串在互斥锁保护下一次性发送）
 * @param str 待发送的字符串指针
 */
void BSP_UART_SendString(const char *str)
{
    uint8_t ch;
    if (str == NULL) return;

    if (s_uart_tx_mutex)
    {
        xSemaphoreTake(s_uart_tx_mutex, pdMS_TO_TICKS(1000));
    }
    while (*str != '\0')
    {
        ch = (uint8_t)(*str);
        HAL_UART_Transmit(&huart1, &ch, 1, 100);
        str++;
    }
    if (s_uart_tx_mutex)
    {
        xSemaphoreGive(s_uart_tx_mutex);
    }
}

/**
 * @brief 发送数据缓冲区（在互斥锁保护下一次性发送）
 * @param buf 数据缓冲区指针
 * @param len 数据长度
 */
void BSP_UART_SendBuf(uint8_t *buf, uint16_t len)
{
    if (s_uart_tx_mutex)
    {
        xSemaphoreTake(s_uart_tx_mutex, pdMS_TO_TICKS(1000));
    }
    HAL_UART_Transmit(&huart1, buf, len, 1000);
    if (s_uart_tx_mutex)
    {
        xSemaphoreGive(s_uart_tx_mutex);
    }
}

/* ========== HAL 回调函数重写 ========== */

/**
 * @brief UART接收完成回调函数
 * @param huart UART句柄指针
 * @note  将接收到的字节存入环形缓冲区，并重新开启中断接收
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        /* 将接收到的字节写入环形缓冲区 */
        RB_Write(&uart_rx_rb, &uart_rx_byte, 1);

        /* 重新开启单字节中断接收 */
        HAL_UART_Receive_IT(&huart1, &uart_rx_byte, 1);
    }
}
