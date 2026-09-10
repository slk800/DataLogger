/**
 * @file    ui_types.h
 * @brief   OLED UI shared types.
 */

#ifndef __UI_TYPES_H
#define __UI_TYPES_H

#include <stdint.h>

typedef enum {
    UI_PAGE_HOME = 0,
    UI_PAGE_STATUS,
    UI_PAGE_FLASH,
    UI_PAGE_LOG,
    UI_PAGE_CONFIG,
    UI_PAGE_ACTION,
    UI_PAGE_COUNT
} UiPage_e;

typedef enum {
    UI_MODE_BROWSE = 0,
    UI_MODE_EDIT,
    UI_MODE_CONFIRM
} UiMode_e;

typedef enum {
    UI_ACTION_NONE = 0,
    UI_ACTION_SAVE_CONFIG,
    UI_ACTION_ERASE_DATA,
    UI_ACTION_REBOOT
} UiAction_e;

typedef struct {
    uint32_t tick_ms;
    uint32_t run_seconds;
    uint32_t sample_count;
    int32_t latest_temp_x10;
    uint16_t adc_raw;
    uint16_t voltage_mv;
    uint32_t resistance_ohm;
    uint8_t ntc_status;
    char latest_msg[40];
    uint32_t log_dropped;
    uint32_t flash_used;
    uint32_t flash_total;
    uint8_t file_count;
    uint32_t heap_free;
    uint32_t heap_min;
    uint32_t sample_ms;
    uint32_t log_level;
    char device_name[16];
    uint8_t flash_ok;
    uint8_t fs_ok;
    uint8_t oled_ok;
} UiStatus_t;

#endif /* __UI_TYPES_H */
