/**
 * @file    cli.h
 * @brief   CLI命令行系统头文件 - 宏定义、类型定义与接口声明
 * @author  SLK800
 * @date    2026-05-01
 * @version 1.0
 */

#ifndef __CLI_H
#define __CLI_H

#include "global.h"

/* ========== 宏定义 ========== */
#define CLI_LINE_MAX    80      /* 单行最大字符数 */
#define CLI_MAX_ARGS    8       /* 最大参数数量 */
#define CLI_MAX_CMDS    20      /* 命令表最大容量 */

/* ========== 类型定义 ========== */

/** @brief 命令处理函数类型 */
typedef void (*CLICmdHandler)(int argc, char *argv[]);

/** @brief 命令条目结构体 */
typedef struct {
    const char   *name;         /* 命令名 */
    CLICmdHandler handler;      /* 处理函数 */
    const char   *help;         /* 帮助说明 */
} CLICommand_t;

/* ========== 函数声明 ========== */

/**
 * @brief 初始化CLI系统
 * @note  注册所有命令，打印欢迎信息
 */
void CLI_Init(void);

/**
 * @brief 解析并执行一行命令
 * @param line 待执行的命令行字符串
 */
void CLI_Execute(char *line);

/**
 * @brief 注册一条命令到命令表
 * @param name    命令名
 * @param handler 处理函数
 * @param help    帮助说明
 */
void CLI_Register(const char *name, CLICmdHandler handler, const char *help);

/**
 * @brief 打印命令提示符
 */
void CLI_PrintPrompt(void);

/**
 * @brief 获取命令表指针（供help命令使用）
 * @param count 输出命令数量
 * @return 命令表指针
 */
const CLICommand_t *CLI_GetCmdTable(uint8_t *count);

#endif /* __CLI_H */
