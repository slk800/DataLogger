/**
 * @file    cli_cmd.h
 * @brief   CLI命令注册头文件
 * @author  SLK800
 * @date    2026-05-01
 * @version 1.0
 */

#ifndef __CLI_CMD_H
#define __CLI_CMD_H

/**
 * @brief 注册所有CLI命令到命令表
 * @note  在CLI_Init中调用，一次性注册所有可用命令
 */
void CLI_RegisterAllCmds(void);

#endif /* __CLI_CMD_H */
