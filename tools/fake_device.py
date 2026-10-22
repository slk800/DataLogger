#!/usr/bin/env python3
"""纯软件假的 DataLogger 设备，用于在没有硬件时验证 dltool / 协议。

它模拟：
  - HELLO / LS / STAT / READ / CRC / GET_CFG / SET_CFG / SAVE_CFG / ERASE
  - SET_TIME / GET_TIME / EVENTS
  - 30% 概率丢弃响应帧（验证重传）
  - 5% 概率在帧前插入垃圾字节（验证流解析重同步）

用法：
  # 终端 A：启动假设备，把 pyserial 的 loop:// 作为虚拟串口
  python tools/fake_device.py --port loop:// --drop 0.3

  # 终端 B：用 pyserial 的 loop 端口跑工具
  python tools/dltool.py --port loop:// info
"""

from __future__ import annotations

import argparse
import io
import random
import struct
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import dlproto as P  # noqa: E402


class FakeFs:
    """与设备端 FS 语义一致（文件级 CRC32 使用 IEEE 802.3，与固件 Utils_CRC32 相同）。"""

    def __init__(self) -> None:
        self.files: dict[str, tuple[int, int, bytes]] = {}  # name -> (type, size, data)
        self.fault = 0
        self.boots = 3
        self.dropped = 0
        self.utc = 0
        self.mkt_x100 = 0
        self.alarm_active = 0
        self.event_count = 0

    def create(self, name: str, ftype: int, data: bytes) -> None:
        self.files[name] = (ftype, len(data), data)

    def find(self, name: str):
        return self.files.get(name)


def make_sample_bytes(count: int) -> bytes:
    """生成与固件 SampleRecord_t 布局一致的测试数据。"""
    out = bytearray()
    for i in range(count):
        tick = 1000 * (i + 1)
        temp = 250 + (i % 20) - 10
        adc = 2000 + (i % 50)
        mv = 1600 + (i % 40)
        body = struct.pack("<IIiHHB3x", tick, i, temp, adc, mv, 0)
        crc = zlib.crc32(body) & 0xFFFFFFFF
        out += body + struct.pack("<I", crc)
    return bytes(out)


class FakeDevice:
    def __init__(self, fs: FakeFs, drop_rate: float, noise_rate: float, seed: int) -> None:
        self.fs = fs
        self.drop_rate = drop_rate
        self.noise_rate = noise_rate
        self.rng = random.Random(seed)
        self.buf = bytearray()

    # ---------- 命令 ----------
    def handle(self, frame: P.Frame) -> tuple[bool, bytes]:
        """返回 (is_nak, payload)。is_nak=True 时 payload 是单字节错误码。"""
        t = frame.type
        if t == P.T_HELLO:
            return False, struct.pack("<BBBBBBH", 1, 0, 0, 0, P.VER, 0x01, P.MAX_DATA)
        if t == P.T_LS:
            return False, self._ls()
        if t == P.T_STAT:
            used = sum(sz for _, sz, _ in self.fs.files.values())
            # 与固件 Cmd_Stat 的 16 字节布局一致
            return False, struct.pack("<IIBBIBB", used, 8 * 1024 * 1024 - 0x8000,
                                      len(self.fs.files), self.fs.fault,
                                      self.fs.dropped & 0xFF, self.fs.boots, 0)
        if t == P.T_READ:
            return self._read(frame)
        if t == P.T_CRC:
            name = frame.data[:16].split(bytes([0]), 1)[0].decode(errors="replace")
            ent = self.fs.find(name)
            if not ent:
                return True, bytes([0x03])
            return False, struct.pack("<II", ent[1], zlib.crc32(ent[2]) & 0xFFFFFFFF)
        if t == P.T_ERASE:
            if frame.data[:4] != P.ERASE_MAGIC:
                return True, bytes([0x08])
            self.fs.files.clear()
            return False, b""
        if t == P.T_GET_CFG:
            return False, {
                1: b"1000", 2: b"1", 3: b"DataLogger",
                4: b"200", 5: b"800", 6: b"50", 7: b"60",
            }.get(frame.data[0], b"")
        if t == P.T_SET_CFG:
            return False, b""
        if t == P.T_SAVE_CFG:
            return False, b""
        if t == P.T_SET_TIME:
            if len(frame.data) < 4:
                return True, bytes([0x02])
            self.fs.utc = struct.unpack_from("<I", frame.data, 0)[0]
            return False, b""
        if t == P.T_GET_TIME:
            return False, struct.pack("<IhBB", self.fs.utc, self.fs.mkt_x100,
                                      self.fs.alarm_active,
                                      1 if self.fs.utc == 0 else 0)
        if t == P.T_EVENTS:
            events = self.fs.find("events")
            size = events[1] if events else 0
            return False, struct.pack("<II", size, self.fs.event_count)
        return True, bytes([0x06])

    def _ls(self) -> bytes:
        out = bytearray()
        for name, (ftype, size, _) in self.fs.files.items():
            rec = bytearray(24)
            nb = name.encode()[:15]
            rec[:len(nb)] = nb
            rec[16] = ftype
            struct.pack_into("<I", rec, 20, size)
            out += rec
        return bytes(out[:P.MAX_DATA])

    def _read(self, frame: P.Frame) -> tuple[bool, bytes]:
        if len(frame.data) < 22:
            return True, bytes([0x02])
        name = frame.data[:16].split(bytes([0]), 1)[0].decode(errors="replace")
        offset, want = struct.unpack_from("<IH", frame.data, 16)
        ent = self.fs.find(name)
        if not ent:
            return True, bytes([0x03])
        data = ent[2]
        if offset >= len(data) or want == 0:
            return True, bytes([0x02])
        return False, data[offset:offset + min(want, P.MAX_DATA)]

    # ---------- 传输 ----------
    def feed(self, chunk: bytes) -> bytes:
        """喂入主机字节，返回设备要发送的字节。"""
        self.buf.extend(chunk)
        out = bytearray()

        for frame in P.parse_stream(self.buf):
            if self.rng.random() < self.drop_rate:
                continue    # 模拟丢帧，主机应重传

            is_nak, payload = self.handle(frame)
            ftype = P.T_NAK if is_nak else P.T_ACK
            pkt = P.Frame(ftype, frame.seq, payload).build()

            # 随机插入一段以定界符结尾的垃圾，验证主机重同步能力
            if self.rng.random() < self.noise_rate:
                out += bytes(self.rng.randrange(1, 256) for _ in range(3))
                out += bytes([P.DELIMITER])

            out += pkt

        return bytes(out)


def run_loop(port: str, args) -> int:
    import serial

    fs = FakeFs()
    fs.create("sample", P.FDATA, make_sample_bytes(args.samples))
    fs.create("log", P.FLOG, b"\x00" * 800)
    fs.utc = args.utc

    dev = FakeDevice(fs, args.drop, args.noise, args.seed)
    ser = serial.serial_for_url(port, 115200, timeout=0.02)
    print(f"fake device on {port}: {len(fs.files)} files, "
          f"sample={args.samples} records", flush=True)

    try:
        while True:
            chunk = ser.read(256)
            if not chunk:
                continue
            resp = dev.feed(chunk)
            if resp:
                ser.write(resp)
    except KeyboardInterrupt:
        print("\nfake device stopped")
        return 0


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="Fake DataLogger device")
    ap.add_argument("--port", default="loop://")
    ap.add_argument("--samples", type=int, default=2000)
    ap.add_argument("--drop", type=float, default=0.3, help="frame drop probability")
    ap.add_argument("--noise", type=float, default=0.05, help="garbage byte probability")
    ap.add_argument("--seed", type=int, default=1)
    ap.add_argument("--utc", type=int, default=1792000000,
                    help="initial wall clock (0 = unset)")
    args = ap.parse_args(argv)
    return run_loop(args.port, args)


if __name__ == "__main__":
    sys.exit(main())
