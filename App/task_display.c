/**
 * @file    task_display.c
 * @brief   OLED 显示与按键任务
 * @author  SLK800
 * @date    2026-05
 * @version 1.1
 *
 * @note
 * 刷新策略（W3）：
 *   - 按键 EXTI 唤醒、报警状态变化、菜单动作 → 立即刷新
 *   - 其余时间按 OLED_REFRESH_MS 刷新
 *   - 连续 OLED_IDLE_FRAMES_MAX 帧无变化后降频到 OLED_IDLE_REFRESH_MS
 *
 * 降频的意义：常态下每秒刷新一次 1 KB 帧缓冲（I2C 400 kHz 约 30 ms
 * @ 15 mA），平均 450 µA；降到 5 s 一次后约 90 µA。
 */

#include "global.h"
#include "task_display.h"

#define KEY_SCAN_PERIOD_MS       50U
#define OLED_REFRESH_MS          1000U
#define OLED_IDLE_REFRESH_MS     5000U
#define OLED_IDLE_FRAMES_MAX     6U
#define FLASH_STATUS_REFRESH_MS  2000U
#define FLASH_ACTION_LOCK_MS     10000U
#define OLED_RECOVERY_MS         1000U

static uint8_t s_last_alarm_state;
static uint8_t s_idle_frames;

static void Task_Display_UpdateFlashStatus(void)
{
    uint32_t used = 0U;
    uint32_t total = 0U;

    FS_GetStatus(&used, &total);

    UI_Status_UpdateFlash(used, total, (uint8_t)FS_GetFileCount());
}

static void Task_Display_RunAction(UiAction_e action)
{
    if (action == UI_ACTION_SAVE_CONFIG)
    {
        (void)Config_Save();
        UI_Status_UpdateSystem();
        return;
    }

    if (action == UI_ACTION_ERASE_DATA)
    {
        if (BSP_Flash_Lock(FLASH_ACTION_LOCK_MS))
        {
            if (FS_Format() == FS_OK)
            {
                FS_LoadIndexCache();
                (void)Log_EnsureStorageFiles();
                Log_RequestSampleCounterReload();
            }
            BSP_Flash_Unlock();
        }
        Task_Display_UpdateFlashStatus();
        return;
    }

    if (action == UI_ACTION_REBOOT)
    {
        (void)Config_Save();
        NVIC_SystemReset();
    }
}

void Task_Display(void *pvParam)
{
    TickType_t last_flash_update;
    TickType_t last_oled_recovery;
    TickType_t last_oled_update;
    UiStatus_t status;

    (void)pvParam;

    BSP_Key_Init();
    UI_Menu_Init();

    UI_Status_SetOledOk(SSD1306_Init() == HAL_OK ? 1U : 0U);
    UI_Status_UpdateSystem();
    Task_Display_UpdateFlashStatus();

    s_last_alarm_state = 0U;
    s_idle_frames = 0U;

    last_flash_update = xTaskGetTickCount();
    last_oled_recovery = last_flash_update;
    last_oled_update = 0U;

    for (;;)
    {
        TickType_t now = xTaskGetTickCount();
        KeyEvent_e key_event = BSP_Key_Scan((uint32_t)now);
        UiAction_e action;
        uint8_t update_oled;

        UI_Status_UpdateSystem();

        /* 按键 EXTI 唤醒：立即刷新，不等 1 s 周期 */
        if (BSP_Key_TakeWakeFlag() != 0U)
        {
            last_oled_update = 0U;
        }

        if ((now - last_flash_update) >= pdMS_TO_TICKS(FLASH_STATUS_REFRESH_MS))
        {
            Task_Display_UpdateFlashStatus();
            last_flash_update = now;
        }

        UI_Status_GetSnapshot(&status);
        action = UI_Menu_HandleKey(key_event, &status);

        /* 报警状态变化必须立即刷新，不能等下一个周期 */
        if (status.alarm_state != s_last_alarm_state)
        {
            s_last_alarm_state = status.alarm_state;
            last_oled_update = 0U;
        }

        {
            uint32_t period = (s_idle_frames >= OLED_IDLE_FRAMES_MAX)
                            ? OLED_IDLE_REFRESH_MS
                            : OLED_REFRESH_MS;
            update_oled = ((now - last_oled_update) >= pdMS_TO_TICKS(period)) ? 1U : 0U;
        }

        if (key_event != KEY_EVENT_NONE && action == UI_ACTION_NONE)
        {
            UI_Status_UpdateSystem();
            UI_Status_GetSnapshot(&status);
            update_oled = 1U;
        }
        if (action != UI_ACTION_NONE)
        {
            Task_Display_RunAction(action);
            UI_Status_GetSnapshot(&status);
            update_oled = 1U;
        }

        if (SSD1306_IsReady() == 0U &&
            (now - last_oled_recovery) >= pdMS_TO_TICKS(OLED_RECOVERY_MS))
        {
            UI_Status_SetOledOk(SSD1306_Recover() == HAL_OK ? 1U : 0U);
            last_oled_recovery = now;
            update_oled = 1U;
        }

        if (update_oled != 0U)
        {
            UI_Menu_Render(&status);

            if (SSD1306_IsReady() != 0U)
            {
                if (SSD1306_UpdateScreen() != HAL_OK)
                {
                    UI_Status_SetOledOk(0U);
                }
            }

            last_oled_update = now;

            /* 连续若干帧内容无变化后降频 */
            if (key_event == KEY_EVENT_NONE && action == UI_ACTION_NONE)
            {
                if (s_idle_frames < 0xFFU)
                {
                    s_idle_frames++;
                }
            }
            else
            {
                s_idle_frames = 0U;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(KEY_SCAN_PERIOD_MS));
    }
}
