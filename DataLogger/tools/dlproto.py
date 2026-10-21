"""DataLogger 串口协议编解码（与固件 Protocol/proto_frame.c 对应）。

纯 Python，无第三方依赖。dltool.py 与 fake_device.py 共用。
"""

from __future__ import annotations

import struct
from dataclasses import dataclass, field

# ---------------- 常量 ----------------
VER = 0x01
DELIMITER = 0x00
HEADER_SIZE = 6
MAX_DATA = 256
MAX_FRAME = MAX_DATA + HEADER_SIZE + 2

T_HELLO = 0x01
T_LS = 0x02
T_STAT = 0x03
T_READ = 0x04
T_CRC = 0x05
T_EVENTS = 0x06
T_GET_CFG = 0x07
T_SET_CFG = 0x08
T_SAVE_CFG = 0x09
T_ERASE = 0x0A
T_SET_TIME = 0x0B
T_GET_TIME = 0x0C
T_ACK = 0x80
T_NAK = 0x81

TYPE_NAMES = {
    T_HELLO: "HELLO", T_LS: "LS", T_STAT: "STAT", T_READ: "READ",
    T_CRC: "CRC", T_EVENTS: "EVENTS", T_GET_CFG: "GET_CFG",
    T_SET_CFG: "SET_CFG", T_SAVE_CFG: "SAVE_CFG", T_ERASE: "ERASE",
    T_SET_TIME: "SET_TIME", T_GET_TIME: "GET_TIME",
    T_ACK: "ACK", T_NAK: "NAK",
}

ERR_NAMES = {
    0x00: "OK", 0x01: "BAD_CRC", 0x02: "BAD_LEN", 0x03: "NO_FILE",
    0x04: "NO_SPACE", 0x05: "FLASH", 0x06: "UNSUPPORTED",
    0x07: "BUSY", 0x08: "DENIED",
}

CFG_KEYS = {1: "sample_ms", 2: "log_level", 3: "name",
            4: "t_low", 5: "t_high", 6: "t_hyst", 7: "t_dur"}
CFG_IDS = {v: k for k, v in CFG_KEYS.items()}

NAME_LEN = 16
LS_RECORD = 24
ERASE_MAGIC = b"ERAS"

# 文件类型
FLOG, FCONFIG, FDATA, FEVENTS = 0, 1, 2, 3
TYPE_TEXT = {FLOG: "LOG", FCONFIG: "CONFIG", FDATA: "DATA", FEVENTS: "EVENTS"}


class ProtoError(Exception):
    pass


class NakError(ProtoError):
    def __init__(self, code: int):
        self.code = code
        super().__init__(f"device returned NAK: {ERR_NAMES.get(code, hex(code))}")


# ---------------- CRC / COBS ----------------
def crc16(data: bytes) -> int:
    """CRC-16/CCITT-FALSE。"""
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def cobs_encode(src: bytes) -> bytes:
    """标准 COBS 编码。

    最后一个块的 code 字节必须写出，否则结尾的 0x00 会在解码时丢失。
    """
    out = bytearray([0])      # 占位：第一个块的 code 字节
    code_idx = 0
    code = 1
    for byte in src:
        if byte == 0:
            out[code_idx] = code
            code_idx = len(out)
            out.append(0)
            code = 1
        else:
            out.append(byte)
            code += 1
            if code == 0xFF:
                out[code_idx] = code
                code_idx = len(out)
                out.append(0)
                code = 1
    out[code_idx] = code
    return bytes(out)


def cobs_decode(src: bytes) -> bytes:
    out = bytearray()
    idx = 0
    while idx < len(src):
        code = src[idx]
        idx += 1
        if code == 0:
            raise ProtoError("zero byte inside COBS frame")
        out.extend(src[idx:idx + code - 1])
        idx += code - 1
        if idx < len(src) and code != 0xFF:
            out.append(0)
    return bytes(out)


# ---------------- 帧 ----------------
@dataclass
class Frame:
    type: int
    seq: int
    data: bytes = b""

    @property
    def name(self) -> str:
        return TYPE_NAMES.get(self.type, f"0x{self.type:02X}")

    def build(self) -> bytes:
        if len(self.data) > MAX_DATA:
            raise ProtoError(f"payload too large: {len(self.data)}")
        raw = struct.pack("<BBHH", VER, self.type, self.seq, len(self.data)) + self.data
        raw += struct.pack("<H", crc16(raw))
        return cobs_encode(raw) + b"\x00"


def parse_frame(raw: bytes) -> Frame:
    decoded = cobs_decode(raw)
    if len(decoded) < HEADER_SIZE + 2:
        raise ProtoError("frame too short")
    ver, ftype, seq, dlen = struct.unpack_from("<BBHH", decoded, 0)
    if ver != VER:
        raise ProtoError(f"unsupported protocol version {ver}")
    if len(decoded) != HEADER_SIZE + dlen + 2:
        raise ProtoError("length mismatch")
    body, crc_bytes = decoded[:-2], decoded[-2:]
    if crc16(body) != struct.unpack("<H", crc_bytes)[0]:
        raise ProtoError("CRC mismatch")
    return Frame(ftype, seq, decoded[HEADER_SIZE:HEADER_SIZE + dlen])


def parse_stream(buf: bytearray) -> list[Frame]:
    """从接收缓冲中切出所有完整帧，并移除已消费的数据。"""
    frames = []
    while True:
        try:
            end = buf.index(DELIMITER)
        except ValueError:
            break
        chunk = bytes(buf[:end])
        del buf[:end + 1]
        if not chunk:
            continue
        try:
            frames.append(parse_frame(chunk))
        except ProtoError:
            continue
    return frames


# ---------------- 请求构造 ----------------
def hello() -> bytes:
    return Frame(T_HELLO, 0).build()


def ls() -> bytes:
    return Frame(T_LS, 0).build()


def stat() -> bytes:
    return Frame(T_STAT, 0).build()


def crc(name: str) -> bytes:
    return Frame(T_CRC, 0, name.encode().ljust(NAME_LEN, b"\0")[:NAME_LEN]).build()


def read(name: str, offset: int, length: int) -> bytes:
    payload = name.encode().ljust(NAME_LEN, b"\0")[:NAME_LEN]
    payload += struct.pack("<IH", offset, length)
    return Frame(T_READ, 0, payload).build()


def get_cfg(key: int) -> bytes:
    return Frame(T_GET_CFG, 0, bytes([key])).build()


def set_cfg(key: int, value: str) -> bytes:
    return Frame(T_SET_CFG, 0, bytes([key]) + value.encode()).build()


def save_cfg() -> bytes:
    return Frame(T_SAVE_CFG, 0).build()


def erase() -> bytes:
    return Frame(T_ERASE, 0, ERASE_MAGIC).build()


def set_time(utc: int) -> bytes:
    return Frame(T_SET_TIME, 0, struct.pack("<I", utc)).build()


def get_time() -> bytes:
    return Frame(T_GET_TIME, 0).build()


def events() -> bytes:
    return Frame(T_EVENTS, 0).build()


# ---------------- 响应解析 ----------------
@dataclass
class FileInfo:
    name: str
    ftype: int
    size: int


def parse_ls(data: bytes) -> list[FileInfo]:
    out = []
    for off in range(0, len(data) - LS_RECORD + 1, LS_RECORD):
        rec = data[off:off + LS_RECORD]
        name = rec[:16].split(b"\0", 1)[0].decode(errors="replace")
        ftype = rec[16]
        size = struct.unpack_from("<I", rec, 20)[0]
        out.append(FileInfo(name, ftype, size))
    return out


@dataclass
class StatInfo:
    used: int = 0
    total: int = 0
    files: int = 0
    last_fault: int = 0
    dropped: int = 0


def parse_stat(data: bytes) -> StatInfo:
    if len(data) < 15:
        raise ProtoError("STAT payload too short")
    used, total = struct.unpack_from("<II", data, 0)
    return StatInfo(used, total, data[8], data[9],
                    struct.unpack_from("<I", data, 10)[0])


def parse_hello(data: bytes) -> dict:
    if len(data) < 8:
        raise ProtoError("HELLO payload too short")
    return {
        "version": f"{data[0]}.{data[1]}.{data[2]}",
        "protocol": data[4],
        "caps": data[5],
        "max_data": struct.unpack_from("<H", data, 6)[0],
    }


def parse_crc(data: bytes) -> tuple[int, int]:
    if len(data) < 8:
        raise ProtoError("CRC payload too short")
    return struct.unpack_from("<II", data, 0)


def parse_cfg(data: bytes) -> str:
    return data.decode(errors="replace")


@dataclass
class TimeInfo:
    utc: int = 0
    mkt_x100: int = 0
    alarm_active: bool = False
    clock_unset: bool = False


def parse_time(data: bytes) -> TimeInfo:
    if len(data) < 8:
        raise ProtoError("GET_TIME payload too short")
    utc = struct.unpack_from("<I", data, 0)[0]
    mkt = struct.unpack_from("<h", data, 4)[0]
    return TimeInfo(utc, mkt, bool(data[6]), bool(data[7]))


def parse_events(data: bytes) -> tuple[int, int]:
    if len(data) < 8:
        raise ProtoError("EVENTS payload too short")
    return struct.unpack_from("<II", data, 0)
