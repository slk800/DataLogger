#!/usr/bin/env python3
"""DataLogger 上位机工具。

子命令：
  info     握手 + 状态
  ls       列出设备文件
  dump     导出文件（支持断点续传，默认导出 sample）
  verify   导出并校验 CRC
  plot     从导出的 CSV 画温度曲线
  sync     设置配置项（sample_ms / log_level / name）
  erase    擦除全部数据

用法示例：
  python tools/dltool.py --port COM3 info
  python tools/dltool.py --port COM3 dump --name sample -o sample.bin
  python tools/dltool.py --port COM3 verify --name sample
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import dlproto as P  # noqa: E402

DEFAULT_TIMEOUT = 0.5
RETRIES = 8
CHUNK = 256


# ---------------- 传输层 ----------------
class Link:
    """串口传输 + 停等重传。"""

    def __init__(self, port: str, baud: int = 115200, timeout: float = DEFAULT_TIMEOUT,
                 verbose: bool = False):
        self.timeout = timeout
        self.verbose = verbose
        self._seq = 0
        self._buf = bytearray()

        if port == "sim":
            raise ValueError("use fake_device for simulation")

        import serial  # pyserial
        self.ser = serial.Serial(port, baud, timeout=0.02)
        self.ser.reset_input_buffer()
        time.sleep(0.2)

    def close(self) -> None:
        self.ser.close()

    def _next_seq(self) -> int:
        self._seq = (self._seq + 1) & 0xFFFF
        return self._seq

    def request(self, ftype: int, data: bytes = b"") -> bytes:
        """发送请求并等待同序号 ACK/NAK，失败重传。"""
        seq = self._next_seq()
        pkt = P.Frame(ftype, seq, data).build()
        name = P.TYPE_NAMES.get(ftype, hex(ftype))

        for attempt in range(RETRIES):
            self.ser.write(pkt)
            deadline = time.monotonic() + self.timeout

            while time.monotonic() < deadline:
                chunk = self.ser.read(64)
                if chunk:
                    self._buf.extend(chunk)

                for frame in P.parse_stream(self._buf):
                    if self.verbose:
                        print(f"  <- {frame.name} seq={frame.seq} len={len(frame.data)}",
                              file=sys.stderr)
                    if frame.seq != seq:
                        continue
                    if frame.type == P.T_NAK:
                        code = frame.data[0] if frame.data else 0
                        raise P.NakError(code)
                    if frame.type == P.T_ACK:
                        return frame.data
                time.sleep(0.005)

            if self.verbose:
                print(f"  timeout on {name} (attempt {attempt + 1})", file=sys.stderr)

        raise P.ProtoError(f"no response to {name} after {RETRIES} attempts")


# ---------------- 命令实现 ----------------
def cmd_info(link: Link, _args) -> int:
    hello = P.parse_hello(link.request(P.T_HELLO))
    stat = P.parse_stat(link.request(P.T_STAT))
    print(f"firmware   : {hello['version']}  protocol {hello['protocol']}")
    print(f"max frame  : {hello['max_data']} bytes")
    print(f"storage    : {stat.used} / {stat.total} bytes used, {stat.files} files")
    print(f"last fault : {stat.last_fault}")
    print(f"log dropped: {stat.dropped}")
    return 0


def cmd_ls(link: Link, _args) -> int:
    files = P.parse_ls(link.request(P.T_LS))
    if not files:
        print("(no files)")
        return 0
    print(f"{'name':<16} {'type':<8} {'bytes':>10}")
    print("-" * 36)
    for f in files:
        print(f"{f.name:<16} {P.TYPE_TEXT.get(f.ftype, '?'):<8} {f.size:>10}")
    return 0


def _crc_name(link: Link, name: str) -> tuple[int, int]:
    return P.parse_crc(link.request(P.T_CRC, name.encode().ljust(16, b"\0")[:16]))


def cmd_dump(link: Link, args) -> int:
    name = args.name
    out = Path(args.output or f"{name}.bin")
    state_path = out.with_suffix(out.suffix + ".state")

    size, dev_crc = _crc_name(link, name)
    if size == 0:
        print(f"error: file '{name}' is empty or missing", file=sys.stderr)
        return 1

    print(f"{name}: {size} bytes, device CRC32 = 0x{dev_crc:08X}")

    # 断点续传
    offset = 0
    if args.resume and state_path.exists() and out.exists():
        try:
            state = json.loads(state_path.read_text())
            if state.get("size") == size and out.stat().st_size == state.get("offset"):
                offset = state["offset"]
                print(f"resuming at {offset} bytes")
        except (json.JSONDecodeError, KeyError):
            pass

    mode = "r+b" if offset else "wb"
    with open(out, mode) as fh:
        if offset:
            fh.seek(offset)
        else:
            fh.truncate(0)

        start = time.monotonic()
        last_report = start

        while offset < size:
            want = min(CHUNK, size - offset)
            payload = name.encode().ljust(16, b"\0")[:16] + struct.pack("<IH", offset, want)
            data = link.request(P.T_READ, payload)
            if not data:
                print(f"\nerror: empty READ at offset {offset}", file=sys.stderr)
                return 1
            fh.write(data)
            offset += len(data)
            fh.flush()

            now = time.monotonic()
            if now - last_report >= 0.5:
                pct = offset * 100.0 / size
                rate = offset / max(now - start, 1e-6) / 1024
                print(f"\r  {offset}/{size} ({pct:5.1f}%) {rate:5.1f} KB/s",
                      end="", flush=True)
                last_report = now

            if args.resume:
                state_path.write_text(json.dumps({"size": size, "offset": offset}))

    print(f"\ndone in {time.monotonic() - start:.1f}s -> {out}")

    if state_path.exists():
        state_path.unlink()

    local_crc = _crc32_file(out)
    if local_crc == dev_crc:
        print(f"CRC32 OK (0x{local_crc:08X})")
        return 0

    print(f"CRC32 MISMATCH: device 0x{dev_crc:08X}, local 0x{local_crc:08X}",
          file=sys.stderr)
    return 1


def _crc32_file(path: Path) -> int:
    import zlib
    crc = 0
    with open(path, "rb") as fh:
        while chunk := fh.read(4096):
            crc = zlib.crc32(chunk, crc)
    return crc & 0xFFFFFFFF


def cmd_verify(link: Link, args) -> int:
    name = args.name
    tmp = Path(args.output or f"{name}.bin")
    rc = cmd_dump(link, argparse.Namespace(name=name, output=str(tmp), resume=False))
    if rc != 0:
        return rc
    print("verify: OK")
    return 0


def cmd_plot(link: Link, args) -> int:
    """把导出的 sample 二进制转 CSV 并画图。"""
    src = Path(args.input)
    raw = src.read_bytes()
    rows = _decode_samples(raw)

    csv_path = Path(args.csv or src.with_suffix(".csv"))
    with open(csv_path, "w", encoding="utf-8", newline="") as fh:
        fh.write("index,tick_ms,temp_c,adc_raw,voltage_mv,status\n")
        for i, r in enumerate(rows):
            fh.write(f"{i},{r['tick']},{r['temp'] / 10.0:.1f},"
                     f"{r['adc']},{r['mv']},{r['status']}\n")
    print(f"wrote {len(rows)} samples -> {csv_path}")

    if not rows:
        return 1

    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib not installed; CSV only", file=sys.stderr)
        return 0

    ticks = [r["tick"] / 1000.0 for r in rows]
    temps = [r["temp"] / 10.0 for r in rows]
    plt.figure(figsize=(10, 4))
    plt.plot(ticks, temps, linewidth=1)
    plt.xlabel("uptime (s)")
    plt.ylabel("temperature (C)")
    plt.title("DataLogger samples")
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    out = Path(args.image or src.with_suffix(".png"))
    plt.savefig(out, dpi=120)
    print(f"plot -> {out}")
    return 0


def _decode_samples(raw: bytes) -> list[dict]:
    """SampleRecord_t: u32 tick, u32 count, i32 temp_x10, u16 adc, u16 mv, u8 status, u8[3], u32 crc = 24B"""
    import zlib
    out = []
    rec = 24
    for off in range(0, len(raw) - rec + 1, rec):
        chunk = raw[off:off + rec]
        tick, count, temp, adc, mv, status = struct.unpack_from("<IIiHHB", chunk, 0)
        crc = struct.unpack_from("<I", chunk, 20)[0]
        if (zlib.crc32(chunk[:20]) & 0xFFFFFFFF) != crc:
            continue
        out.append({"tick": tick, "count": count, "temp": temp,
                    "adc": adc, "mv": mv, "status": status})
    return out


def cmd_sync(link: Link, args) -> int:
    key_id = P.CFG_IDS[args.key]
    if args.value is not None:
        link.request(P.T_SET_CFG, bytes([key_id]) + args.value.encode())
        link.request(P.T_SAVE_CFG)
        print(f"{args.key} = {args.value} (saved)")
    else:
        val = P.parse_cfg(link.request(P.T_GET_CFG, bytes([key_id])))
        print(f"{args.key} = {val}")
    return 0


def cmd_time(link: Link, args) -> int:
    """读取或设置设备时钟。"""
    if args.set is not None:
        utc = _parse_time_arg(args.set)
        link.request(P.T_SET_TIME, struct.pack("<I", utc))
        print(f"clock set to {utc} ({time.strftime('%Y-%m-%d %H:%M:%S', time.gmtime(utc))} UTC)")
        return 0

    info = P.parse_time(link.request(P.T_GET_TIME))
    if info.clock_unset:
        print("clock : NOT SET")
    else:
        print(f"utc   : {info.utc} ({time.strftime('%Y-%m-%d %H:%M:%S', time.gmtime(info.utc))})")
    print(f"mkt   : {info.mkt_x100 / 100.0:.2f} C")
    print(f"alarm : {'ACTIVE' if info.alarm_active else 'normal'}")
    return 0


def _parse_time_arg(value: str) -> int:
    """接受 'now'、Unix 秒，或 'YYYY-MM-DD HH:MM:SS'（按 UTC 解释）。"""
    if value == "now":
        return int(time.time())
    if value.isdigit():
        return int(value)
    try:
        parts = time.strptime(value, "%Y-%m-%d %H:%M:%S")
        import calendar
        return calendar.timegm(parts)
    except ValueError as exc:
        raise SystemExit(f"bad time format: {value!r}") from exc


def cmd_events(link: Link, args) -> int:
    size, count = P.parse_events(link.request(P.T_EVENTS))
    print(f"events file : {size} bytes ({size // 15} records)")
    print(f"events since boot: {count}")
    if args.dump and size:
        name = "events"
        out = Path(args.output or "events.bin")
        rc = cmd_dump(link, argparse.Namespace(name=name, output=str(out), resume=False))
        if rc == 0:
            print(f"wrote {out}")
            _print_events(out)
        return rc
    return 0


def _print_events(path: Path) -> None:
    raw = path.read_bytes()
    import zlib
    print(f"{'kind':<6} {'start':<20} {'end':<20} {'min':>7} {'max':>7}")
    for off in range(0, len(raw) - 14, 15):
        chunk = raw[off:off + 15]
        kind = chunk[0]
        start, end = struct.unpack_from("<II", chunk, 1)
        tmin, tmax = struct.unpack_from("<hh", chunk, 9)
        if (zlib.crc32(chunk[:13]) & 0xFFFF) != struct.unpack_from("<H", chunk, 13)[0]:
            continue
        print(f"{'HIGH' if kind == 1 else 'LOW':<6} "
              f"{time.strftime('%m-%d %H:%M:%S', time.gmtime(start)):<20} "
              f"{time.strftime('%m-%d %H:%M:%S', time.gmtime(end)):<20} "
              f"{tmin / 100:>7.2f} {tmax / 100:>7.2f}")


def cmd_erase(link: Link, args) -> int:
    if not args.yes:
        answer = input("erase ALL data? type 'yes': ")
        if answer.strip() != "yes":
            print("cancelled")
            return 1
    link.request(P.T_ERASE, P.ERASE_MAGIC)
    print("erased")
    return 0


# ---------------- 入口 ----------------
def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="DataLogger serial tool")
    ap.add_argument("--port", required=True, help="serial port, e.g. COM3 or /dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--timeout", type=float, default=DEFAULT_TIMEOUT)
    ap.add_argument("-v", "--verbose", action="store_true")

    sub = ap.add_subparsers(dest="cmd", required=True)

    sub.add_parser("info", help="handshake and show status").set_defaults(func=cmd_info)
    sub.add_parser("ls", help="list files").set_defaults(func=cmd_ls)

    p_dump = sub.add_parser("dump", help="export a file")
    p_dump.add_argument("--name", default="sample")
    p_dump.add_argument("-o", "--output")
    p_dump.add_argument("--resume", action="store_true", help="resume from .state")
    p_dump.set_defaults(func=cmd_dump)

    p_verify = sub.add_parser("verify", help="export and check CRC")
    p_verify.add_argument("--name", default="sample")
    p_verify.add_argument("-o", "--output")
    p_verify.set_defaults(func=cmd_verify)

    p_plot = sub.add_parser("plot", help="decode exported sample file")
    p_plot.add_argument("input", help="raw .bin exported by dump")
    p_plot.add_argument("--csv")
    p_plot.add_argument("--image")
    p_plot.set_defaults(func=cmd_plot)

    p_sync = sub.add_parser("sync", help="get/set config")
    p_sync.add_argument("key", choices=sorted(P.CFG_IDS))
    p_sync.add_argument("value", nargs="?")
    p_sync.set_defaults(func=cmd_sync)

    p_time = sub.add_parser("time", help="get/set device clock")
    p_time.add_argument("--set", metavar="WHEN",
                        help="'now', Unix seconds, or 'YYYY-MM-DD HH:MM:SS'")
    p_time.set_defaults(func=cmd_time)

    p_events = sub.add_parser("events", help="list alarm events")
    p_events.add_argument("--dump", action="store_true", help="download the events file")
    p_events.add_argument("-o", "--output")
    p_events.set_defaults(func=cmd_events)

    p_erase = sub.add_parser("erase", help="erase all data")
    p_erase.add_argument("--yes", action="store_true")
    p_erase.set_defaults(func=cmd_erase)

    args = ap.parse_args(argv)

    try:
        link = Link(args.port, args.baud, args.timeout, args.verbose)
    except ImportError:
        print("pyserial not installed: pip install pyserial", file=sys.stderr)
        return 2

    try:
        return args.func(link, args)
    except P.NakError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    except P.ProtoError as exc:
        print(f"protocol error: {exc}", file=sys.stderr)
        return 1
    finally:
        link.close()


if __name__ == "__main__":
    sys.exit(main())
