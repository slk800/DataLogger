/**
 * @file    fault.c
 * @brief   故障记录与复位原因实现
 * @author  SLK800
 * @date    2026-05
 * @version 1.0
 *
 * @note
 * 存储位置选择：STM32F103 的 BKP 备份寄存器（BKP_DR1 起）位于备份域，
 *      只要 VBAT/3V3 不停电就一直保留；软件复位、看门狗复位、NRST 复位
 *      都不会清除它。用它保存"上一次故障码 / 任务名缩写 / 启动次数"，
 *      重启之后仍然可读，这是本模块能提供跨复位诊断的原因。
 *
 *      BKP 寄存器在 F1 上是 16 位宽，写入前需：
 *        1) 打开 PWR 与 BKP 时钟
 *        2) 置位 PWR_CR.DBP 解除备份域写保护
 */

#include "fault.h"
#include "stm32f1xx_hal.h"

/* ========== BKP 寄存器分配 ========== */
#define FAULT_BKP_CODE_REG      BKP->DR1     /* bit15: 有效标志, bit14: 任务名有效, bit3:0 故障码 */
#define FAULT_BKP_NAME_REG      BKP->DR2     /* 任务名前两个字节 */
#define FAULT_BKP_BOOT_REG      BKP->DR3     /* 启动次数，饱和于 0x7FFF */
#define FAULT_BKP_MAGIC         0x8000U      /* 有效标志 */

/* ========== 运行期现场 ========== */
volatile uint32_t g_fault_code = FAULT_CODE_NONE;
volatile uint32_t g_fault_task_handle = 0U;
volatile char     g_fault_task_name[16];

static uint32_t s_last_code = FAULT_CODE_NONE;
static char     s_last_name[3] = {0, 0, 0};
static uint32_t s_reset_flags = 0U;
static uint32_t s_boot_count = 0U;

/* ========== 私有函数 ========== */

static void Fault_BackupDomainUnlock(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_RCC_BKP_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
}

/* ========== 公共函数 ========== */

void Fault_HookRecord(uint32_t code, uint32_t task, const char *name)
{
    uint16_t code_reg = (uint16_t)(FAULT_BKP_MAGIC | (code & 0x000FU));
    uint16_t name_reg = 0U;

    g_fault_code = code;
    g_fault_task_handle = task;

    /*
     * 钩子内不调用任何 HAL/RTOS 服务。
     * BKP 写保护在 Fault_Init() 中已经解除，这里只做两次寄存器写。
     */
    if (name != NULL && name[0] != '\0')
    {
        name_reg = (uint16_t)(uint8_t)name[0];
        if (name[1] != '\0')
        {
            name_reg |= (uint16_t)((uint16_t)(uint8_t)name[1] << 8);
        }
        code_reg |= 0x4000U;   /* 任务名有效 */
    }

    BKP->DR1 = code_reg;
    BKP->DR2 = name_reg;
}

void Fault_Init(void)
{
    uint16_t code_reg;

    /* 1. 先保存本次复位原因（读 RCC_CSR），再清标志 */
    s_reset_flags = RCC->CSR;
    __HAL_RCC_CLEAR_RESET_FLAGS();

    /* 2. 解除备份域写保护，供后续故障钩子直接写 */
    Fault_BackupDomainUnlock();

    /* 3. 读回上一次的故障记录（先清空，避免沿用旧值） */
    s_last_code = FAULT_CODE_NONE;
    s_last_name[0] = '\0';
    s_last_name[1] = '\0';
    s_last_name[2] = '\0';

    code_reg = (uint16_t)FAULT_BKP_CODE_REG;
    if ((code_reg & FAULT_BKP_MAGIC) != 0U)
    {
        s_last_code = (uint32_t)(code_reg & 0x000FU);
        if ((code_reg & 0x4000U) != 0U)
        {
            uint16_t name_reg = (uint16_t)FAULT_BKP_NAME_REG;
            s_last_name[0] = (char)(name_reg & 0xFFU);
            s_last_name[1] = (char)((name_reg >> 8) & 0xFFU);
        }
    }

    /* 4. 启动计数 */
    s_boot_count = (uint32_t)FAULT_BKP_BOOT_REG;
    if (s_boot_count < 0x7FFFU)
    {
        s_boot_count++;
        FAULT_BKP_BOOT_REG = (uint16_t)s_boot_count;
    }

    /*
     * 5. 清空"上一次故障"槽位。
     *    本次运行如果再次进入故障钩子，会重新写入；正常运行时该槽位为 0，
     *    因此 OLED / CLI 读到的永远是"上一次运行"的真实故障。
     */
    FAULT_BKP_CODE_REG = 0U;
    FAULT_BKP_NAME_REG = 0U;

    g_fault_code = FAULT_CODE_NONE;
    g_fault_task_handle = 0U;
    g_fault_task_name[0] = '\0';
}

uint32_t Fault_GetLastCode(void)
{
    return s_last_code;
}

const char *Fault_GetLastTaskName(void)
{
    return s_last_name;
}

uint32_t Fault_GetResetFlags(void)
{
    return s_reset_flags;
}

uint32_t Fault_GetBootCount(void)
{
    return s_boot_count;
}

const char *Fault_CodeName(uint32_t code)
{
    switch (code)
    {
    case FAULT_CODE_NONE:
        return "NONE";
    case FAULT_CODE_STACK_OVERFLOW:
        return "STACKOVF";
    case FAULT_CODE_MALLOC_FAILED:
        return "MALLOC";
    default:
        return "UNKNOWN";
    }
}
