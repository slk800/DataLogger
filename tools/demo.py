#!/usr/bin/env python3
"""无硬件演示：用 fake_device + MemLink 跑通完整流程并生成报告。

用途：
  - 没有实物时录制演示视频/GIF
  - 每次改协议后做冒烟测试
  - 面试时现场演示（不依赖板子和驱动）

流程：
  1. 握手、读状态
  2. 设置时钟
  3. 列出文件
  4. 导出 sample 并校验 CRC
  5. 导出 events
  6. 生成 HTML 报告

用法：
  python tools/demo.py --out demo/            # 输出到 demo/
  python tools/demo.py --out demo/ --records 2000 --events 12
"""

from __future__ import annotations

import argparse
import struct
import sys
import time
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "tests"))

import dlproto as P          # noqa: E402
import dlreport              # noqa: E402
import fake_device as FD     # noqa: E402

# 复用端到端测试的内存链路（无串口依赖）
from e2e_test import MemLink   # noqa: E402  (tests/ 已在 sys.path)


def synth_records(n: int, base_utc: int, step: int = 60) -> bytes:
    """生成带一次高温超限的 v2 记录序列。"""
    out = bytearray()
    temp = 5.0
    for i in range(n):
        # 缓慢漂移 + 第 40%~50% 区间高温
        temp += 0.02 if i % 7 else -0.13
        t = temp
        if n * 2 // 5 <= i < n // 2:
            t = 9.4
        rec = struct.pack("<IhhHBBH", base_utc + i * step,
                          int(t * 100), -32768, 3300, 0, i & 0xFF, 0)
        crc = 0xFFFF
        for b in rec:
            crc ^= b << 8
            for _ in range(8):
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
        out += rec + struct.pack("<H", crc)
    return bytes(out)


def synth_events(n: int, base_utc: int) -> bytes:
    out = bytearray()
    for i in range(n):
        start = base_utc + i * 3600
        end = start + 600
        body = struct.pack("<BIIhh", 1 if i % 2 == 0 else 2, start, end, 850, 940)
        crc = 0xFFFF
        for b in body:
            crc ^= b << 8
            for _ in range(8):
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
        out += body + struct.pack("<H", crc)
    return bytes(out)


def step(msg: str) -> None:
    print(f"\n\033[1m== {msg}\033[0m")


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="Hardware-free DataLogger demo")
    ap.add_argument("--out", default="demo", help="output directory")
    ap.add_argument("--records", type=int, default=600)
    ap.add_argument("--events", type=int, default=8)
    ap.add_argument("--drop", type=float, default=0.2, help="simulated frame loss")
    ap.add_argument("--utc", type=int, default=1792000000)
    args = ap.parse_args(argv)

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    # ---- 构造假设备 ----
    sample = synth_records(args.records, args.utc)
    events = synth_events(args.events, args.utc)
    fs = FD.FakeFs()
    fs.create("sample", P.FDATA, sample)
    fs.create("log", P.FLOG, b"\x00" * 512)
    fs.create("events", P.FEVENTS, events)
    fs.utc = args.utc
    fs.event_count = args.events
    fs.mkt_x100 = 640

    dev = FD.FakeDevice(fs, drop_rate=args.drop, noise_rate=0.02, seed=42)
    link = MemLink(dev, retries=20)

    # ---- 1. 握手 ----
    step("1. 握手")
    hello = P.parse_hello(link.request(P.T_HELLO))
    print(f"  固件 {hello['version']}  协议 {hello['protocol']}  "
          f"最大帧 {hello['max_data']} B")

    # ---- 2. 状态 ----
    step("2. 状态")
    st = P.parse_stat(link.request(P.T_STAT))
    print(f"  存储 {st.used} / {st.total} B，{st.files} 个文件")

    # ---- 3. 时钟 ----
    step("3. 时钟")
    ti = P.parse_time(link.request(P.T_GET_TIME))
    print(f"  UTC {ti.utc} ({time.strftime('%Y-%m-%d %H:%M:%S', time.gmtime(ti.utc))})")
    print(f"  MKT {ti.mkt_x100 / 100:.2f} °C")

    # ---- 4. 文件列表 ----
    step("4. 文件列表")
    for f in P.parse_ls(link.request(P.T_LS)):
        print(f"  {f.name:<10} {P.TYPE_TEXT.get(f.ftype, '?'):<8} {f.size:>8} B")

    # ---- 5. 导出 sample ----
    step("5. 导出 sample（带丢帧重传）")
    size, dev_crc = P.parse_crc(link.request(P.T_CRC, b"sample".ljust(16, b"\0")))
    got = bytearray()
    t0 = time.monotonic()
    while len(got) < size:
        want = min(P.MAX_DATA, size - len(got))
        payload = b"sample".ljust(16, b"\0") + struct.pack("<IH", len(got), want)
        got += link.request(P.T_READ, payload)
    dt = time.monotonic() - t0
    local_crc = zlib.crc32(got) & 0xFFFFFFFF
    print(f"  {size} B in {dt:.2f}s ({size / dt / 1024:.1f} KB/s)")
    print(f"  设备 CRC 0x{dev_crc:08X} / 本地 CRC 0x{local_crc:08X} "
          f"{'OK' if dev_crc == local_crc else 'MISMATCH'}")
    if dev_crc != local_crc:
        return 1
    (out / "sample.bin").write_bytes(got)

    # ---- 6. 导出 events ----
    step("6. 导出 events")
    ev_size, ev_count = P.parse_events(link.request(P.T_EVENTS))
    print(f"  {ev_size} B，本次上电 {ev_count} 条")
    if ev_size:
        got_ev = bytearray()
        while len(got_ev) < ev_size:
            want = min(P.MAX_DATA, ev_size - len(got_ev))
            payload = b"events".ljust(16, b"\0") + struct.pack("<IH", len(got_ev), want)
            got_ev += link.request(P.T_READ, payload)
        (out / "events.bin").write_bytes(got_ev)
        print(f"  -> {out / 'events.bin'}")

    # ---- 7. 生成报告 ----
    step("7. 生成报告")
    samples = dlreport.load_samples(out / "sample.bin")
    intervals = dlreport.replay_alarm(samples, 200, 800, 50, 60)
    evs = dlreport.load_events(out / "events.bin") if (out / "events.bin").exists() else []

    class Args:
        low, high, hyst, min_duration = 200, 800, 50, 60
        title = "运输温度报告（演示数据）"
        input = str(out / "sample.bin")

    html = dlreport.render_html(samples, intervals, evs, Args)
    (out / "report.html").write_text(html, encoding="utf-8")

    temps = [s.t1 / 100.0 for s in samples if s.t1 != -32768]
    print(f"  {len(samples)} 条记录")
    print(f"  min {min(temps):.2f}  max {max(temps):.2f}  "
          f"mean {dlreport.mean(temps):.2f}  MKT {dlreport.mkt(temps):.2f} °C")
    print(f"  超限区间 {len(intervals)}  设备事件 {len(evs)}")
    print(f"  -> {out / 'report.html'}")

    step("完成")
    print(f"  产物目录：{out.resolve()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
