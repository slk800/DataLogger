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
    uint32_t last_fault_code;      /* 上一次复位前的故障码，0 = 正常 */
    uint32_t boot_count;           /* 本次上电后的启动次数（BKP 保留） */
    uint32_t reset_flags;          /* 本次复位的 RCC_CSR 原始标志 */
    uint32_t utc;                  /* 当前墙钟（0 = 未设置） */
    int16_t  mkt_x100;             /* MKT，摄氏度 ×100 */
    uint8_t  alarm_state;          /* 0=正常, 1=高温, 2=低温 */
    uint16_t alarm_low_x100;
    uint16_t alarm_high_x100;
    uint32_t event_count;
} UiStatus_t;

#endif /* __UI_TYPES_H */
