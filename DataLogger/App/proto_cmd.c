/**
 * @file    proto_cmd.c
 * @brief   协议命令分发实现
 * @author  SLK800
 * @date    2026-05
 * @version 1.0
 *
 * @note
 * 帧层的错误（CRC、版本、格式）已在 Proto_ParseFrame 处理；
 * 本文件只负责语义错误，统一用 NAK + 错误码回复。
 * 所有访问 W25Q64 的命令都由 Task_Proto 在 Flash 锁内调用。
 */

#include "proto_cmd.h"
#include "fs_core.h"
#include "fs_file.h"
#include "fs_types.h"
#include "config.h"
#include "log.h"
#include "fault.h"
#include "bsp_flash.h"
#include "task_recorder.h"
#include <string.h>

/* ========== LS 记录布局（24 字节） ========== */
#define LS_RECORD_SIZE      24U
#define LS_MAX_RECORDS      (PROTO_MAX_DATA / LS_RECORD_SIZE)   /* 10 */

/* ========== 配置键 ID ========== */
#define CFG_KEY_SAMPLE_MS   1U
#define CFG_KEY_LOG_LEVEL   2U
#define CFG_KEY_NAME        3U
#define CFG_KEY_T_LOW       4U
#define CFG_KEY_T_HIGH      5U
#define CFG_KEY_T_HYST      6U
#define CFG_KEY_T_DUR       7U

/* ========== 文件名长度 ========== */
#define PROTO_NAME_LEN      16U

/* ========== 私有辅助 ========== */

static uint32_t PutU32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFU);
    p[1] = (uint8_t)((v >> 8) & 0xFFU);
    p[2] = (uint8_t)((v >> 16) & 0xFFU);
    p[3] = (uint8_t)((v >> 24) & 0xFFU);
    return 4U;
}

static uint32_t GetU32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint16_t GetU16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

/**
 * @brief 从帧载荷里复制文件名并保证以 '\0' 结尾
 * @return true 表示文件名合法（非空且不超长）
 */
static bool ReadName(const uint8_t *src, char *dst)
{
    uint32_t i;

    for (i = 0U; i < PROTO_NAME_LEN; i++)
    {
        dst[i] = (char)src[i];
        if (dst[i] == '\0')
        {
            break;
        }
    }
    dst[PROTO_NAME_LEN] = '\0';

    return (i > 0U) && (i < PROTO_NAME_LEN);
}

/* ========== 各命令处理 ========== */

static uint32_t Cmd_Hello(const ProtoFrame_t *req, uint8_t *out, uint32_t cap)
{
    uint8_t data[8];

    (void)req;

    data[0] = 1U;       /* major */
    data[1] = 0U;       /* minor */
    data[2] = 0U;       /* patch */
    data[3] = 0U;
    data[4] = PROTO_VER;
    data[5] = 0x01U;    /* 能力：文件系统 */
    data[6] = (uint8_t)(PROTO_MAX_DATA & 0xFFU);
    data[7] = (uint8_t)((PROTO_MAX_DATA >> 8) & 0xFFU);

    return Proto_BuildAck(req->seq, data, sizeof(data), out, cap);
}

static uint32_t Cmd_Ls(const ProtoFrame_t *req, uint8_t *out, uint32_t cap)
{
    FileEntry_t list[FS_FILE_MAX];
    uint8_t count = 0U;
    uint8_t data[PROTO_MAX_DATA];
    uint32_t pos = 0U;
    uint8_t i;

    if (FS_File_List(list, FS_FILE_MAX, &count) != FS_OK)
    {
        return Proto_BuildNak(req->seq, PROTO_ERR_FLASH, out, cap);
    }

    for (i = 0U; i < count; i++)
    {
        uint8_t *rec;

        if ((pos + LS_RECORD_SIZE) > sizeof(data))
        {
            count = i;      /* 单帧容量不足，截断并如实返回 */
            break;
        }

        rec = &data[pos];
        memset(rec, 0, LS_RECORD_SIZE);
        memcpy(rec, list[i].name, FS_FILENAME_MAX);

        rec[16] = list[i].type;
        rec[17] = 0U;
        rec[18] = 0U;
        rec[19] = 0U;
        (void)PutU32(&rec[20], list[i].size);

        pos += LS_RECORD_SIZE;
    }

    return Proto_BuildAck(req->seq, data, (uint16_t)pos, out, cap);
}

static uint32_t Cmd_Stat(const ProtoFrame_t *req, uint8_t *out, uint32_t cap)
{
    uint8_t data[16];
    uint32_t used = 0U;
    uint32_t total = 0U;
    uint32_t dropped;
    uint8_t  file_count = 0U;
    FileEntry_t list[FS_FILE_MAX];
    uint8_t i;

    FS_GetStatus(&used, &total);
    if (FS_File_List(list, FS_FILE_MAX, &file_count) != FS_OK)
    {
        file_count = 0U;
    }

    memset(data, 0, sizeof(data));
    (void)PutU32(&data[0], used);
    (void)PutU32(&data[4], total);
    data[8] = file_count;
    data[9] = (uint8_t)Fault_GetLastCode();

    dropped = g_log_dropped;
    data[10] = (uint8_t)(dropped & 0xFFU);
    data[11] = (uint8_t)((dropped >> 8) & 0xFFU);
    data[12] = (uint8_t)((dropped >> 16) & 0xFFU);
    data[13] = (uint8_t)((dropped >> 24) & 0xFFU);

    data[14] = (uint8_t)Fault_GetBootCount();
    data[15] = 0U;

    (void)i;

    return Proto_BuildAck(req->seq, data, sizeof(data), out, cap);
}

static uint32_t Cmd_Read(const ProtoFrame_t *req, uint8_t *out, uint32_t cap)
{
    char     name[PROTO_NAME_LEN + 1U];
    uint32_t offset;
    uint16_t want;
    uint32_t size;
    uint32_t avail;
    uint8_t  data[PROTO_MAX_DATA];

    if (req->len < (PROTO_NAME_LEN + 6U))
    {
        return Proto_BuildNak(req->seq, PROTO_ERR_BAD_LEN, out, cap);
    }
    if (!ReadName(req->data, name))
    {
        return Proto_BuildNak(req->seq, PROTO_ERR_BAD_LEN, out, cap);
    }

    offset = GetU32(&req->data[PROTO_NAME_LEN]);
    want   = GetU16(&req->data[PROTO_NAME_LEN + 4U]);

    if (want == 0U || want > PROTO_MAX_DATA)
    {
        return Proto_BuildNak(req->seq, PROTO_ERR_BAD_LEN, out, cap);
    }

    size = FS_File_GetSize(name);
    if (size == 0U)
    {
        return Proto_BuildNak(req->seq, PROTO_ERR_NO_FILE, out, cap);
    }
    if (offset >= size)
    {
        return Proto_BuildNak(req->seq, PROTO_ERR_BAD_LEN, out, cap);
    }

    avail = size - offset;
    if (avail > want)
    {
        avail = want;
    }

    if (FS_File_ReadRange(name, offset, data, avail) != FS_OK)
    {
        return Proto_BuildNak(req->seq, PROTO_ERR_FLASH, out, cap);
    }

    return Proto_BuildAck(req->seq, data, (uint16_t)avail, out, cap);
}

static uint32_t Cmd_Crc(const ProtoFrame_t *req, uint8_t *out, uint32_t cap)
{
    char        name[PROTO_NAME_LEN + 1U];
    FileEntry_t entry;
    uint8_t     data[8];

    if (req->len < PROTO_NAME_LEN)
    {
        return Proto_BuildNak(req->seq, PROTO_ERR_BAD_LEN, out, cap);
    }
    if (!ReadName(req->data, name))
    {
        return Proto_BuildNak(req->seq, PROTO_ERR_BAD_LEN, out, cap);
    }
    if (FS_File_Find(name, &entry, NULL) != FS_OK)
    {
        return Proto_BuildNak(req->seq, PROTO_ERR_NO_FILE, out, cap);
    }

    (void)PutU32(&data[0], entry.size);
    (void)PutU32(&data[4], entry.crc);

    return Proto_BuildAck(req->seq, data, sizeof(data), out, cap);
}

static uint32_t Cmd_GetCfg(const ProtoFrame_t *req, uint8_t *out, uint32_t cap)
{
    char key[16];
    char val[32];

    if (req->len < 1U)
    {
        return Proto_BuildNak(req->seq, PROTO_ERR_BAD_LEN, out, cap);
    }

    switch (req->data[0])
    {
    case CFG_KEY_SAMPLE_MS: (void)strcpy(key, "sample_ms"); break;
    case CFG_KEY_LOG_LEVEL: (void)strcpy(key, "log_level"); break;
    case CFG_KEY_NAME:      (void)strcpy(key, "name");      break;
    case CFG_KEY_T_LOW:     (void)strcpy(key, "t_low");     break;
    case CFG_KEY_T_HIGH:    (void)strcpy(key, "t_high");    break;
    case CFG_KEY_T_HYST:    (void)strcpy(key, "t_hyst");    break;
    case CFG_KEY_T_DUR:     (void)strcpy(key, "t_dur");     break;
    default:                return Proto_BuildNak(req->seq, PROTO_ERR_UNSUPPORTED, out, cap);
    }

    if (Config_Get(key, val, sizeof(val)) != HAL_OK)
    {
        return Proto_BuildNak(req->seq, PROTO_ERR_UNSUPPORTED, out, cap);
    }

    return Proto_BuildAck(req->seq, (const uint8_t *)val,
                          (uint16_t)strlen(val), out, cap);
}

static uint32_t Cmd_SetCfg(const ProtoFrame_t *req, uint8_t *out, uint32_t cap)
{
    char key[16];
    char val[32];
    uint16_t n;

    if (req->len < 2U)
    {
        return Proto_BuildNak(req->seq, PROTO_ERR_BAD_LEN, out, cap);
    }

    switch (req->data[0])
    {
    case CFG_KEY_SAMPLE_MS: (void)strcpy(key, "sample_ms"); break;
    case CFG_KEY_LOG_LEVEL: (void)strcpy(key, "log_level"); break;
    case CFG_KEY_NAME:      (void)strcpy(key, "name");      break;
    case CFG_KEY_T_LOW:     (void)strcpy(key, "t_low");     break;
    case CFG_KEY_T_HIGH:    (void)strcpy(key, "t_high");    break;
    case CFG_KEY_T_HYST:    (void)strcpy(key, "t_hyst");    break;
    case CFG_KEY_T_DUR:     (void)strcpy(key, "t_dur");     break;
    default:                return Proto_BuildNak(req->seq, PROTO_ERR_UNSUPPORTED, out, cap);
    }

    n = req->len - 1U;
    if (n >= sizeof(val))
    {
        return Proto_BuildNak(req->seq, PROTO_ERR_BAD_LEN, out, cap);
    }
    memcpy(val, &req->data[1], n);
    val[n] = '\0';

    if (Config_Set(key, val) != HAL_OK)
    {
        return Proto_BuildNak(req->seq, PROTO_ERR_UNSUPPORTED, out, cap);
    }

    return Proto_BuildAck(req->seq, NULL, 0U, out, cap);
}

static uint32_t Cmd_SaveCfg(const ProtoFrame_t *req, uint8_t *out, uint32_t cap)
{
    if (Config_Save() != HAL_OK)
    {
        return Proto_BuildNak(req->seq, PROTO_ERR_FLASH, out, cap);
    }
    return Proto_BuildAck(req->seq, NULL, 0U, out, cap);
}

static uint32_t Cmd_Erase(const ProtoFrame_t *req, uint8_t *out, uint32_t cap)
{
    static const uint8_t magic[4] = {'E', 'R', 'A', 'S'};

    if (req->len < 4U || memcmp(req->data, magic, 4U) != 0)
    {
        return Proto_BuildNak(req->seq, PROTO_ERR_DENIED, out, cap);
    }

    if (FS_Format() != FS_OK)
    {
        return Proto_BuildNak(req->seq, PROTO_ERR_FLASH, out, cap);
    }

    FS_LoadIndexCache();
    (void)Log_EnsureStorageFiles();
    Log_RequestSampleCounterReload();

    return Proto_BuildAck(req->seq, NULL, 0U, out, cap);
}

static uint32_t Cmd_SetTime(const ProtoFrame_t *req, uint8_t *out, uint32_t cap)
{
    uint32_t utc;

    if (req->len < 4U)
    {
        return Proto_BuildNak(req->seq, PROTO_ERR_BAD_LEN, out, cap);
    }

    utc = GetU32(req->data);
    if (utc == 0U || !Recorder_SetUtc(utc))
    {
        return Proto_BuildNak(req->seq, PROTO_ERR_UNSUPPORTED, out, cap);
    }

    return Proto_BuildAck(req->seq, NULL, 0U, out, cap);
}

static uint32_t Cmd_GetTime(const ProtoFrame_t *req, uint8_t *out, uint32_t cap)
{
    uint8_t  data[8];
    uint32_t utc = Recorder_GetUtc();
    int16_t  mkt = Recorder_GetMkt();
    const AlarmCtx_t *alarm = Recorder_GetAlarm();

    memset(data, 0, sizeof(data));
    (void)PutU32(&data[0], utc);
    data[4] = (uint8_t)(mkt & 0xFFU);
    data[5] = (uint8_t)((mkt >> 8) & 0xFFU);
    data[6] = (alarm != NULL && Alarm_IsActive(alarm)) ? 1U : 0U;
    data[7] = (utc == 0U) ? 1U : 0U;      /* bit: 时钟未设置 */

    return Proto_BuildAck(req->seq, data, sizeof(data), out, cap);
}

static uint32_t Cmd_Events(const ProtoFrame_t *req, uint8_t *out, uint32_t cap)
{
    uint8_t  data[8];
    uint32_t size = FS_File_GetSize("events");

    (void)PutU32(&data[0], size);
    (void)PutU32(&data[4], Recorder_GetEventCount());

    return Proto_BuildAck(req->seq, data, sizeof(data), out, cap);
}

/* ========== 分发 ========== */

uint32_t ProtoCmd_Handle(const ProtoFrame_t *req, uint8_t *out, uint32_t out_cap)
{
    if (req == NULL || out == NULL || out_cap == 0U)
    {
        return 0U;
    }

    switch (req->type)
    {
    case PROTO_T_HELLO:    return Cmd_Hello(req, out, out_cap);
    case PROTO_T_LS:       return Cmd_Ls(req, out, out_cap);
    case PROTO_T_STAT:     return Cmd_Stat(req, out, out_cap);
    case PROTO_T_READ:     return Cmd_Read(req, out, out_cap);
    case PROTO_T_CRC:      return Cmd_Crc(req, out, out_cap);
    case PROTO_T_GET_CFG:  return Cmd_GetCfg(req, out, out_cap);
    case PROTO_T_SET_CFG:  return Cmd_SetCfg(req, out, out_cap);
    case PROTO_T_SAVE_CFG: return Cmd_SaveCfg(req, out, out_cap);
    case PROTO_T_ERASE:    return Cmd_Erase(req, out, out_cap);
    case PROTO_T_SET_TIME: return Cmd_SetTime(req, out, out_cap);
    case PROTO_T_GET_TIME: return Cmd_GetTime(req, out, out_cap);
    case PROTO_T_EVENTS:   return Cmd_Events(req, out, out_cap);
    default:
        return Proto_BuildNak(req->seq, PROTO_ERR_UNSUPPORTED, out, out_cap);
    }
}
