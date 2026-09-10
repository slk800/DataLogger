/**
 * @file    task_display.c
 * @brief   OLED display and key-input task implementation.
 */

#include "global.h"
#include "task_display.h"

#define KEY_SCAN_PERIOD_MS       50U
#define OLED_REFRESH_MS          1000U
#define FLASH_STATUS_REFRESH_MS  2000U
#define FLASH_ACTION_LOCK_MS     10000U
#define OLED_RECOVERY_MS         1000U

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
        if ((now - last_flash_update) >= pdMS_TO_TICKS(FLASH_STATUS_REFRESH_MS))
        {
            Task_Display_UpdateFlashStatus();
            last_flash_update = now;
        }

        UI_Status_GetSnapshot(&status);
        action = UI_Menu_HandleKey(key_event, &status);
        update_oled = ((now - last_oled_update) >= pdMS_TO_TICKS(OLED_REFRESH_MS)) ? 1U : 0U;
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
        }

        vTaskDelay(pdMS_TO_TICKS(KEY_SCAN_PERIOD_MS));
    }
}
