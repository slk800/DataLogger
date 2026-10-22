/**
 * @file    bsp_lowpower.c
 * @brief   STOP 模式进入/退出与时钟恢复
 * @author  SLK800
 * @date    2026-05
 * @version 1.0
 *
 * @note
 * 时钟恢复顺序（RM0008 §7.3）：
 *   1. 开 HSE，等待 HSERDY
 *   2. 配置 PLL（HSE × 9），等待 PLLRDY
 *   3. 切换 SYSCLK 到 PLL
 *   4. 设置 Flash 等待周期（72 MHz 需要 LATENCY=2）
 *   5. SystemCoreClockUpdate()
 *
 * 顺序颠倒（例如先切 SYSCLK 再等 PLL 锁定）会让系统跑在错误时钟上，
 * 表现为串口乱码、SPI 时序异常——这类 bug 很难从现象定位。
 */

#include "bsp_lowpower.h"
#include "ssd1306.h"
#include "bsp_flash.h"

static bool s_rtc_available = false;
static bool s_active = false;

/* ========== 时钟恢复 ========== */

HAL_StatusTypeDef BSP_LowPower_RestoreClock(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    /* 1. HSE + PLL */
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    osc.HSIState = RCC_HSI_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL = RCC_PLL_MUL9;

    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* 2. 总线分频 + SYSCLK 切换 */
    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* 3. 同步 HAL 的时钟变量 */
    SystemCoreClockUpdate();

    return HAL_OK;
}

/* ========== 公共接口 ========== */

void BSP_LowPower_Init(bool rtc_available)
{
    s_rtc_available = rtc_available;
    s_active = false;
}

bool BSP_LowPower_CanStop(void)
{
    /*
     * 没有 RTC 就不能进 STOP：F1 在 STOP 下只有 RTC/IWDG 能运行，
     * SysTick 停止后没有任何东西能把 MCU 按时叫醒。
     */
    return s_rtc_available;
}

HAL_StatusTypeDef BSP_LowPower_EnterStop(void)
{
    if (!s_rtc_available)
    {
        return HAL_ERROR;
    }

    /* 清唤醒标志，否则上一轮的标志会让 WFI 立刻返回 */
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);

    s_active = true;

    /*
     * 进入 STOP，低功耗稳压器。
     * 这里用 HAL_PWR_EnterSTOPMode，返回后系统时钟为 HSI 8 MHz，
     * 紧接着恢复 PLL，保证调用者看到的是 72 MHz。
     */
    HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);

    {
        HAL_StatusTypeDef ret = BSP_LowPower_RestoreClock();
        s_active = false;
        return ret;
    }
}

void BSP_LowPower_DeviceGate(bool sleep)
{
    if (sleep)
    {
        /* OLED 关显示：电荷泵关闭后约 10–20 µA */
        SSD1306_Sleep();

        /* W25Q64 深睡：从 ~4 mA 降到 ~1 µA */
        (void)BSP_Flash_DeepSleep();
    }
    else
    {
        (void)BSP_Flash_Wake();
        (void)SSD1306_Recover();
    }
}

bool BSP_LowPower_IsActive(void)
{
    return s_active;
}
