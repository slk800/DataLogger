/**
 * @file    ui_menu.c
 * @brief   OLED menu state machine and page rendering.
 */

#include "global.h"

static UiPage_e s_page;
static UiMode_e s_mode;
static uint8_t s_cursor;
static uint8_t s_edit_index;

static const uint32_t s_sample_values[] = {100U, 200U, 500U, 1000U, 2000U, 5000U};
static const char *s_level_names[] = {"DEBUG", "INFO", "WARN", "ERROR"};

static uint8_t UI_Menu_FindSampleIndex(uint32_t sample_ms)
{
    uint8_t i;
    for (i = 0U; i < (uint8_t)(sizeof(s_sample_values) / sizeof(s_sample_values[0])); i++)
    {
        if (s_sample_values[i] == sample_ms)
        {
            return i;
        }
    }
    return 3U;
}

void UI_Menu_Init(void)
{
    s_page = UI_PAGE_HOME;
    s_mode = UI_MODE_BROWSE;
    s_cursor = 0U;
    s_edit_index = 0U;
}

UiPage_e UI_Menu_GetPage(void)
{
    return s_page;
}

static uint8_t UI_Menu_MaxCursor(void)
{
    if (s_page == UI_PAGE_CONFIG)
    {
        return 2U;
    }
    if (s_page == UI_PAGE_ACTION)
    {
        return 2U;
    }
    return 0U;
}

static void UI_Menu_MovePage(int8_t delta)
{
    int8_t next = (int8_t)s_page + delta;
    if (next < 0)
    {
        next = UI_PAGE_COUNT - 1;
    }
    if (next >= UI_PAGE_COUNT)
    {
        next = 0;
    }
    s_page = (UiPage_e)next;
    s_cursor = 0U;
}

static void UI_Menu_MoveCursor(int8_t delta)
{
    uint8_t max = UI_Menu_MaxCursor();
    if (max == 0U)
    {
        UI_Menu_MovePage(delta);
        return;
    }

    if (delta < 0)
    {
        s_cursor = (s_cursor == 0U) ? max : (uint8_t)(s_cursor - 1U);
    }
    else
    {
        s_cursor = (s_cursor >= max) ? 0U : (uint8_t)(s_cursor + 1U);
    }
}

UiAction_e UI_Menu_HandleKey(KeyEvent_e event, UiStatus_t *status)
{
    (void)status;

    if (event == KEY_EVENT_NONE)
    {
        return UI_ACTION_NONE;
    }

    if (event == KEY_EVENT_OK_LONG)
    {
        if (s_mode == UI_MODE_CONFIRM)
        {
            s_mode = UI_MODE_BROWSE;
            return UI_ACTION_ERASE_DATA;
        }
        s_page = UI_PAGE_HOME;
        s_mode = UI_MODE_BROWSE;
        s_cursor = 0U;
        return UI_ACTION_NONE;
    }

    if (s_mode == UI_MODE_CONFIRM)
    {
        if (event == KEY_EVENT_UP_SHORT || event == KEY_EVENT_DOWN_SHORT)
        {
            s_mode = UI_MODE_BROWSE;
        }
        return UI_ACTION_NONE;
    }

    if (s_mode == UI_MODE_EDIT)
    {
        if (s_page == UI_PAGE_CONFIG && s_cursor == 0U)
        {
            if (event == KEY_EVENT_UP_SHORT && s_edit_index > 0U)
            {
                s_edit_index--;
            }
            if (event == KEY_EVENT_DOWN_SHORT && s_edit_index < 5U)
            {
                s_edit_index++;
            }
            if (event == KEY_EVENT_OK_SHORT)
            {
                g_config.sample_interval_ms = s_sample_values[s_edit_index];
                s_mode = UI_MODE_BROWSE;
            }
        }
        else if (s_page == UI_PAGE_CONFIG && s_cursor == 1U)
        {
            if (event == KEY_EVENT_UP_SHORT && g_config.log_level > LOG_LEVEL_DEBUG)
            {
                g_config.log_level--;
            }
            if (event == KEY_EVENT_DOWN_SHORT && g_config.log_level < LOG_LEVEL_ERROR)
            {
                g_config.log_level++;
            }
            g_log_level = (LogLevel_e)g_config.log_level;
            if (event == KEY_EVENT_OK_SHORT)
            {
                s_mode = UI_MODE_BROWSE;
            }
        }
        return UI_ACTION_NONE;
    }

    if (event == KEY_EVENT_UP_SHORT)
    {
        UI_Menu_MoveCursor(-1);
    }
    if (event == KEY_EVENT_DOWN_SHORT)
    {
        UI_Menu_MoveCursor(1);
    }

    if (event == KEY_EVENT_OK_SHORT)
    {
        if (s_page == UI_PAGE_CONFIG && s_cursor == 0U)
        {
            s_edit_index = UI_Menu_FindSampleIndex(g_config.sample_interval_ms);
            s_mode = UI_MODE_EDIT;
        }
        else if (s_page == UI_PAGE_CONFIG && s_cursor == 1U)
        {
            s_mode = UI_MODE_EDIT;
        }
        else if (s_page == UI_PAGE_CONFIG && s_cursor == 2U)
        {
            return UI_ACTION_SAVE_CONFIG;
        }
        else if (s_page == UI_PAGE_ACTION && s_cursor == 0U)
        {
            return UI_ACTION_SAVE_CONFIG;
        }
        else if (s_page == UI_PAGE_ACTION && s_cursor == 1U)
        {
            s_mode = UI_MODE_CONFIRM;
        }
        else if (s_page == UI_PAGE_ACTION && s_cursor == 2U)
        {
            return UI_ACTION_REBOOT;
        }
    }

    return UI_ACTION_NONE;
}

static void UI_Menu_Line(uint8_t line, const char *text)
{
    SSD1306_DrawStringAt(0U, line, text == NULL ? "" : text);
}

static void UI_Menu_FormatTime(char *buf, uint8_t len, uint32_t seconds)
{
    uint32_t h = seconds / 3600U;
    uint32_t m = (seconds / 60U) % 60U;
    uint32_t s = seconds % 60U;
    snprintf(buf, len, "%02lu:%02lu:%02lu", h, m, s);
}

static const char *UI_Menu_LevelName(uint32_t level)
{
    if (level > LOG_LEVEL_ERROR)
    {
        level = LOG_LEVEL_INFO;
    }
    return s_level_names[level];
}

static void UI_Menu_RenderHome(const UiStatus_t *st)
{
    char line[24];
    int32_t temp_x10 = st->latest_temp_x10;
    const char *temp_sign = temp_x10 < 0 ? "-" : "";
    int32_t temp_abs = temp_x10 < 0 ? -temp_x10 : temp_x10;

    snprintf(line, sizeof(line), "%s", st->device_name[0] ? st->device_name : "DataLogger");
    UI_Menu_Line(0U, line);
    if (st->ntc_status == SENSOR_NTC_OK)
    {
        snprintf(line, sizeof(line), "Temp:%s%ld.%ld C", temp_sign, (long)(temp_abs / 10), (long)(temp_abs % 10));
    }
    else
    {
        snprintf(line, sizeof(line), "Temp:SENSOR ERR");
    }
    UI_Menu_Line(1U, line);
    snprintf(line, sizeof(line), "Cnt :%lu", st->sample_count);
    UI_Menu_Line(2U, line);
    snprintf(line, sizeof(line), "Tick:%lu", st->tick_ms);
    UI_Menu_Line(3U, line);
    snprintf(line, sizeof(line), "Drop:%lu", st->log_dropped);
    UI_Menu_Line(4U, line);
}

static void UI_Menu_RenderStatus(const UiStatus_t *st)
{
    char line[24];
    UI_Menu_Line(0U, "System");
    snprintf(line, sizeof(line), "Heap:%lu", st->heap_free);
    UI_Menu_Line(1U, line);
    snprintf(line, sizeof(line), "Min :%lu", st->heap_min);
    UI_Menu_Line(2U, line);
    UI_Menu_FormatTime(line, sizeof(line), st->run_seconds);
    UI_Menu_Line(3U, line);
    snprintf(line, sizeof(line), "NTC:%s", Sensor_NTC_StatusName((SensorNtcStatus_e)st->ntc_status));
    UI_Menu_Line(4U, line);
    snprintf(line, sizeof(line), "ADC:%u", st->adc_raw);
    UI_Menu_Line(5U, line);
}

static void UI_Menu_RenderFlash(const UiStatus_t *st)
{
    char line[24];
    uint32_t free_bytes = (st->flash_total > st->flash_used) ? (st->flash_total - st->flash_used) : 0U;

    UI_Menu_Line(0U, "Flash");
    snprintf(line, sizeof(line), "Used:%lu KB", st->flash_used >> 10);
    UI_Menu_Line(1U, line);
    snprintf(line, sizeof(line), "Free:%lu KB", free_bytes >> 10);
    UI_Menu_Line(2U, line);
    snprintf(line, sizeof(line), "Files:%u", st->file_count);
    UI_Menu_Line(3U, line);
}

static void UI_Menu_RenderLog(const UiStatus_t *st)
{
    char line[24];
    UI_Menu_Line(0U, "Log");
    snprintf(line, sizeof(line), "Level:%s", UI_Menu_LevelName(st->log_level));
    UI_Menu_Line(1U, line);
    snprintf(line, sizeof(line), "Drop :%lu", st->log_dropped);
    UI_Menu_Line(2U, line);
    UI_Menu_Line(3U, "Last:");
    UI_Menu_Line(4U, st->latest_msg);
}

static void UI_Menu_RenderConfig(const UiStatus_t *st)
{
    char line[24];
    uint32_t sample_ms = st->sample_ms;

    if (s_mode == UI_MODE_EDIT && s_cursor == 0U)
    {
        sample_ms = s_sample_values[s_edit_index];
    }

    UI_Menu_Line(0U, "Config");
    snprintf(line, sizeof(line), "%c Sample:%lums", s_cursor == 0U ? '>' : ' ', sample_ms);
    UI_Menu_Line(1U, line);
    snprintf(line, sizeof(line), "%c Level:%s", s_cursor == 1U ? '>' : ' ', UI_Menu_LevelName(st->log_level));
    UI_Menu_Line(2U, line);
    snprintf(line, sizeof(line), "%c Save", s_cursor == 2U ? '>' : ' ');
    UI_Menu_Line(3U, line);
    if (s_mode == UI_MODE_EDIT)
    {
        UI_Menu_Line(5U, "Edit:UP/DN OK");
    }
}

static void UI_Menu_RenderAction(void)
{
    char line[24];
    UI_Menu_Line(0U, "Action");
    snprintf(line, sizeof(line), "%c Save Config", s_cursor == 0U ? '>' : ' ');
    UI_Menu_Line(1U, line);
    snprintf(line, sizeof(line), "%c Erase Data", s_cursor == 1U ? '>' : ' ');
    UI_Menu_Line(2U, line);
    snprintf(line, sizeof(line), "%c Reboot", s_cursor == 2U ? '>' : ' ');
    UI_Menu_Line(3U, line);
}

void UI_Menu_Render(const UiStatus_t *status)
{
    if (status == NULL)
    {
        return;
    }

    SSD1306_Clear();
    if (s_mode == UI_MODE_CONFIRM)
    {
        UI_Menu_Line(0U, "Erase all data?");
        UI_Menu_Line(2U, "UP/DOWN:Cancel");
        UI_Menu_Line(3U, "OK long:Confirm");
        return;
    }

    switch (s_page)
    {
    case UI_PAGE_HOME:
        UI_Menu_RenderHome(status);
        break;
    case UI_PAGE_STATUS:
        UI_Menu_RenderStatus(status);
        break;
    case UI_PAGE_FLASH:
        UI_Menu_RenderFlash(status);
        break;
    case UI_PAGE_LOG:
        UI_Menu_RenderLog(status);
        break;
    case UI_PAGE_CONFIG:
        UI_Menu_RenderConfig(status);
        break;
    case UI_PAGE_ACTION:
        UI_Menu_RenderAction();
        break;
    default:
        UI_Menu_RenderHome(status);
        break;
    }
}
