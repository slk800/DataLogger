/**
 * @file    cli.c
 * @brief   CLI命令行系统实现 - 命令注册、解析与执行引擎
 * @author  SLK800
 * @date    2026-05-01
 * @version 1.0
 */

#include "cli.h"
#include "cli_cmd.h"
#include "bsp_uart.h"
#include "utils.h"
#include <string.h>
#include <stdio.h>

/* ========== 静态变量 ========== */
static CLICommand_t s_cmd_table[CLI_MAX_CMDS];
static uint8_t      s_cmd_count = 0;

/* ========== API实现 ========== */

void CLI_Init(void)
{
    s_cmd_count = 0;
    memset(s_cmd_table, 0, sizeof(s_cmd_table));

    CLI_RegisterAllCmds();

    BSP_UART_SendString("\r\n========== DataLogger CLI ==========\r\n");
    CLI_PrintPrompt();
}

void CLI_Register(const char *name, CLICmdHandler handler, const char *help)
{
    if (s_cmd_count < CLI_MAX_CMDS)
    {
        s_cmd_table[s_cmd_count].name    = name;
        s_cmd_table[s_cmd_count].handler = handler;
        s_cmd_table[s_cmd_count].help    = help;
        s_cmd_count++;
    }
}

const CLICommand_t *CLI_GetCmdTable(uint8_t *count)
{
    *count = s_cmd_count;
    return s_cmd_table;
}

void CLI_PrintPrompt(void)
{
    BSP_UART_SendString("> ");
}

void CLI_Execute(char *line)
{
    char  buf[CLI_LINE_MAX];
    char *argv[CLI_MAX_ARGS];
    int   argc;
    uint8_t i;

    /* 复制到局部缓冲区 */
    strncpy(buf, line, CLI_LINE_MAX - 1);
    buf[CLI_LINE_MAX - 1] = '\0';

    /* 去除首尾空格 */
    Utils_StrTrim(buf);

    /* 空行直接返回提示符 */
    if (buf[0] == '\0')
    {
        CLI_PrintPrompt();
        return;
    }

    /* 按空格分割 */
    argc = (int)Utils_StrSplit(buf, ' ', argv, CLI_MAX_ARGS);

    /* 查找匹配命令 */
    for (i = 0; i < s_cmd_count; i++)
    {
        if (strcmp(argv[0], s_cmd_table[i].name) == 0)
        {
            s_cmd_table[i].handler(argc, argv);
            CLI_PrintPrompt();
            return;
        }
    }

    /* 未找到 */
    BSP_UART_SendString("Unknown: ");
    BSP_UART_SendString(argv[0]);
    BSP_UART_SendString(". Type 'help'\r\n");

    CLI_PrintPrompt();
}
