/**
 * @file    task_shell.c
 * @brief   Shell任务 — 串口命令行交互
 * @author  SLK800
 * @date    2026-05-01
 * @version 1.0
 */

#include "global.h"
#include "task_shell.h"

void Task_Shell(void *pvParam)
{
    char    line_buf[CLI_LINE_MAX];
    uint8_t idx = 0;
    uint8_t ch;

    (void)pvParam;

    BSP_UART_SendString("[Shell] started\r\n");
    CLI_Init();

    for (;;)
    {
        if (RB_Read(&uart_rx_rb, &ch, 1) > 0)
        {
            if (ch == '\r' || ch == '\n')
            {
                if (idx > 0)
                {
                    line_buf[idx] = '\0';
                    BSP_UART_SendString("\r\n");
                    CLI_Execute(line_buf);
                    idx = 0;
                }
            }
            else if (ch == '\b' || ch == 0x7F)
            {
                if (idx > 0)
                {
                    idx--;
                    BSP_UART_SendString("\b \b");
                }
            }
            else
            {
                if (idx < CLI_LINE_MAX - 1)
                {
                    line_buf[idx++] = ch;
                    BSP_UART_SendByte(ch);
                }
            }
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}
