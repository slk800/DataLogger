/**
 * @file    task_proto.c
 * @brief   协议任务 - 从串口环形缓冲区取字节，解析帧并回复
 * @author  SLK800
 * @date    2026-05
 * @version 1.0
 *
 * @note
 * 与 Task_Shell 共用同一个 UART 接收环形缓冲区，按首字节分流：
 *   - 0x00 或非可打印字节 → 交给协议解析器
 *   - 可打印字符          → 交给行缓冲，凑满一行后进入 CLI
 *
 * 这样打开二进制协议时，人工调试的文本命令依然可用。
 */

#include "global.h"
#include "task_proto.h"
#include "proto_cmd.h"

#define PROTO_POLL_MS           5U
#define PROTO_FLASH_LOCK_MS     2000U
#define PROTO_LINE_MAX          80U

/* ========== 内部状态 ========== */

static ProtoRx_t  s_rx;
static char       s_line[PROTO_LINE_MAX];
static uint8_t    s_line_len;
static uint8_t    s_resp[PROTO_MAX_FRAME * 2U];   /* 响应帧，静态分配以节省任务栈 */

/**
 * @brief 处理一行文本命令（复用现有 CLI）
 */
static void Task_Proto_HandleLine(void)
{
    if (s_line_len == 0U)
    {
        return;
    }

    s_line[s_line_len] = '\0';
    s_line_len = 0U;

    BSP_UART_SendString("\r\n");
    CLI_Execute(s_line);
}

/**
 * @brief 处理一个已解析的二进制帧
 */
static void Task_Proto_HandleFrame(const ProtoFrame_t *frame)
{
    uint32_t resp_len = 0U;

    /*
     * 只有会访问 Flash 的命令才需要持锁；
     * 这里统一持锁，逻辑简单，代价是可接受的（单帧处理时间 < 100 ms）。
     */
    if (BSP_Flash_Lock(PROTO_FLASH_LOCK_MS))
    {
        resp_len = ProtoCmd_Handle(frame, s_resp, sizeof(s_resp));
        BSP_Flash_Unlock();
    }
    else
    {
        resp_len = Proto_BuildNak(frame->seq, PROTO_ERR_BUSY, s_resp, sizeof(s_resp));
    }

    if (resp_len > 0U)
    {
        BSP_UART_SendBuf(s_resp, (uint16_t)resp_len);
    }
}

/* ========== 任务入口 ========== */

void Task_Proto(void *pvParam)
{
    uint8_t ch;

    (void)pvParam;

    Proto_RxReset(&s_rx);
    s_line_len = 0U;

    /* 命令表在本任务初始化：Shell 任务关闭时文本 CLI 仍可人工调试 */
    CLI_Init();

    BSP_UART_SendString("[Proto] binary protocol ready\r\n");

    for (;;)
    {
        if (RB_Read(&uart_rx_rb, &ch, 1U) > 0U)
        {
            /*
             * 分流规则：
             *   可打印字符或换行/退格 → 文本命令
             *   其余                   → 二进制帧
             * COBS 编码后的数据不会出现 0x00，所以定界符也走二进制分支。
             */
            if (ch == '\r' || ch == '\n')
            {
                Task_Proto_HandleLine();
            }
            else if (ch == '\b' || ch == 0x7FU)
            {
                if (s_line_len > 0U)
                {
                    s_line_len--;
                    BSP_UART_SendString("\b \b");
                }
            }
            else if (ch >= 0x20U && ch <= 0x7EU)
            {
                if (s_line_len < (PROTO_LINE_MAX - 1U))
                {
                    s_line[s_line_len++] = (char)ch;
                    BSP_UART_SendByte(ch);      /* 本地回显 */
                }
            }
            else
            {
                ProtoFrame_t frame;

                if (Proto_RxFeed(&s_rx, ch, &frame) == 1)
                {
                    Task_Proto_HandleFrame(&frame);
                }
            }
        }
        else
        {
            vTaskDelay(pdMS_TO_TICKS(PROTO_POLL_MS));
        }
    }
}
