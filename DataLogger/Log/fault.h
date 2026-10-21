/**
 * @file    fault.h
 * @brief   故障记录与复位原因 - 跨复位保留的只读诊断信息
 * @author  SLK800
 * @date    2026-05
 * @version 1.0
 *
 * @note
 * 故障钩子（栈溢出 / 动态内存申请失败）只写 volatile 全局变量和
 * BKP 备份寄存器，不调用 UART、队列、互斥量或任何可能持锁的服务。
 * BKP 寄存器在复位后仍然保留，因此复位后可以读出"上一次为什么挂掉"。
 */

#ifndef __FAULT_H
#define __FAULT_H

#include <stdint.h>
#include <stddef.h>

/* ========== 故障码定义 ========== */
#define FAULT_CODE_NONE              0U   /* 无记录故障 */
#define FAULT_CODE_STACK_OVERFLOW    1U   /* FreeRTOS 栈溢出 */
#define FAULT_CODE_MALLOC_FAILED     2U   /* FreeRTOS 动态内存申请失败 */

/* ========== 当前运行期故障现场（钩子写入） ========== */
extern volatile uint32_t g_fault_code;
extern volatile uint32_t g_fault_task_handle;
extern volatile char     g_fault_task_name[16];

/* ========== 接口 ========== */

/**
 * @brief 初始化故障诊断模块
 * @note  在 App_Init() 早期、任务创建之前调用。
 *        读取上一次复位前保存的故障码与复位原因，并递增启动计数。
 */
void Fault_Init(void);

/**
 * @brief 故障钩子内的最小记录路径
 * @param code     故障码
 * @param task     任务句柄（可为0）
 * @param name     任务名（可为NULL）
 * @note  只写全局变量和 BKP 寄存器，不调用任何 RTOS/HAL 服务。
 */
void Fault_HookRecord(uint32_t code, uint32_t task, const char *name);

/**
 * @brief 上一次复位前记录的故障码
 * @return FAULT_CODE_NONE 表示上次为正常复位
 */
uint32_t Fault_GetLastCode(void);

/**
 * @brief 上一次故障所在任务名的前两个字符（不足补 '\0'）
 */
const char *Fault_GetLastTaskName(void);

/**
 * @brief 本次复位的原始 RCC_CSR 标志
 * @note  位定义见 stm32f1xx.h：RCC_CSR_PORRSTF / PINRSTF / IWDGRSTF 等
 */
uint32_t Fault_GetResetFlags(void);

/**
 * @brief 累计启动次数（BKP 保留，断电清空）
 */
uint32_t Fault_GetBootCount(void);

/**
 * @brief 故障码 → 简短文本
 */
const char *Fault_CodeName(uint32_t code);

#endif /* __FAULT_H */
