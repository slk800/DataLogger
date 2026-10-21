/*
 * Fault 模块主机侧行为测试。
 *
 * 用同一个进程重复调用 Fault_Init() 模拟"复位重新上电"：
 * BKP 寄存器（host_BKP）在两次调用之间保持不变，
 * 正好对应真实硬件上 BKP 在复位后仍保留的语义。
 */
#include <stdio.h>
#include <string.h>
#include "fault.h"
#include "stm32f1xx_hal.h"

static int failures;

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else         { printf("ok  : %s\n", msg); } \
} while (0)

int main(void)
{
    /* ---- 第 1 次上电：正常启动 ---- */
    RCC->CSR = RCC_CSR_PORRSTF;
    Fault_Init();
    CHECK(Fault_GetBootCount() == 1U, "boot count starts at 1");
    CHECK(Fault_GetLastCode() == FAULT_CODE_NONE, "first boot has no previous fault");
    CHECK((Fault_GetResetFlags() & RCC_CSR_PORRSTF) != 0U, "POR reset flag captured");
    CHECK((RCC->CSR & RCC_CSR_PORRSTF) == 0U, "reset flags cleared after read");

    /* ---- 运行中发生栈溢出 ---- */
    Fault_HookRecord(FAULT_CODE_STACK_OVERFLOW, 0x1234U, "DataProd");
    CHECK(g_fault_code == FAULT_CODE_STACK_OVERFLOW, "hook records runtime fault code");

    /* ---- 第 2 次上电（BKP 保留） ---- */
    RCC->CSR = RCC_CSR_IWDGRSTF;
    Fault_Init();
    CHECK(Fault_GetBootCount() == 2U, "boot count increments");
    CHECK(Fault_GetLastCode() == FAULT_CODE_STACK_OVERFLOW, "previous fault survives reset");
    CHECK(strcmp(Fault_GetLastTaskName(), "Da") == 0, "previous fault task name survives reset");
    CHECK((Fault_GetResetFlags() & RCC_CSR_IWDGRSTF) != 0U, "IWDG reset flag captured");

    /* ---- 第 3 次上电：上一次是正常复位 ---- */
    RCC->CSR = RCC_CSR_SFTRSTF;
    Fault_Init();
    CHECK(Fault_GetBootCount() == 3U, "boot count keeps incrementing");
    CHECK(Fault_GetLastCode() == FAULT_CODE_NONE, "fault slot cleared after clean run");

    /* ---- 堆失败路径 ---- */
    Fault_HookRecord(FAULT_CODE_MALLOC_FAILED, 0U, NULL);
    RCC->CSR = RCC_CSR_PORRSTF;
    Fault_Init();
    CHECK(Fault_GetLastCode() == FAULT_CODE_MALLOC_FAILED, "malloc fault survives reset");
    CHECK(strcmp(Fault_GetLastTaskName(), "") == 0, "malloc fault has no task name");

    printf("\nfault_test: %s\n", failures ? "FAILED" : "PASSED");
    return failures ? 1 : 0;
}
