/**
 * @file    proto_cmd.h
 * @brief   协议命令分发 - 把解析后的帧映射到 FS / Config / Log
 * @author  SLK800
 * @date    2026-05
 * @version 1.0
 */

#ifndef __PROTO_CMD_H
#define __PROTO_CMD_H

#include "proto_frame.h"

/**
 * @brief 处理一帧请求并生成响应
 * @param req    请求帧
 * @param out    响应缓冲区
 * @param out_cap 缓冲区容量
 * @return 写入 out 的字节数；0 表示无需响应
 * @note  调用者负责持有 Flash 锁（部分命令会访问 W25Q64）。
 */
uint32_t ProtoCmd_Handle(const ProtoFrame_t *req, uint8_t *out, uint32_t out_cap);

#endif /* __PROTO_CMD_H */
