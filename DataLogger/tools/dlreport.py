"""从导出的 sample 文件生成运输温度报告（CSV + HTML）。

报告内容：
  - 样本数、时间跨度、采样间隔
  - 温度 min / max / mean / MKT
  - 超限区间（按上下限 + 回差重放报警状态机）
  - 内嵌 SVG 曲线图（不依赖 matplotlib）
  - 可选的报警事件表（来自 events 文件）

用法：
  python tools/dlreport.py sample.bin -o report.html
  python tools/dlreport.py sample.bin --events events.bin --low 200 --high 800
"""

from __future__ import annotations

import argparse
import html
import math
import struct
import sys
import time
import zlib
from dataclasses import dataclass, field
from pathlib import Path

RECORD_SIZE = 16
EA_OVER_R = 10000.0
KELVIN = 273.15


@dataclass
class Sample:
    ts: int
    t1: int          # 摄氏度 ×100
    t2: int
    vbat: int
    flags: int
    seq: int
    aux: int


@dataclass
class Event:
    kind: int
    start: int
    end: int
    tmin: int
    tmax: int


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def load_samples(path: Path) -> list[Sample]:
    raw = path.read_bytes()
    out: list[Sample] = []
    bad = 0

    for off in range(0, len(raw) - RECORD_SIZE + 1, RECORD_SIZE):
        chunk = raw[off:off + RECORD_SIZE]
        if chunk == b"\xff" * RECORD_SIZE:
            break                       # 擦除区，数据到此为止
        if crc16(chunk[:14]) != struct.unpack_from("<H", chunk, 14)[0]:
            bad += 1
            continue
        ts, t1, t2, vbat, flags, seq, aux = struct.unpack("<IhhHBBH", chunk[:14])
        out.append(Sample(ts, t1, t2, vbat, flags, seq, aux))

    if bad:
        print(f"warning: skipped {bad} record(s) with bad CRC", file=sys.stderr)
    return out


def load_events(path: Path) -> list[Event]:
    raw = path.read_bytes()
    out: list[Event] = []
    for off in range(0, len(raw) - 14, 15):
        chunk = raw[off:off + 15]
        if chunk == b"\xff" * 15:
            break
        if crc16(chunk[:13]) != struct.unpack_from("<H", chunk, 13)[0]:
            continue
        kind = chunk[0]
        start, end = struct.unpack_from("<II", chunk, 1)
        tmin, tmax = struct.unpack_from("<hh", chunk, 9)
        out.append(Event(kind, start, end, tmin, tmax))
    return out


# ---------------- 统计 ----------------

def mean(values: list[float]) -> float:
    return sum(values) / len(values) if values else 0.0


def mkt(temps_c: list[float]) -> float:
    """Mean Kinetic Temperature（与固件 domain_mkt.c 同一公式）。"""
    if not temps_c:
        return 0.0
    s = sum(math.exp(-EA_OVER_R / (t + KELVIN)) for t in temps_c)
    return -EA_OVER_R / math.log(s / len(temps_c)) - KELVIN


@dataclass
class Interval:
    kind: str           # HIGH / LOW
    start: int
    end: int
    tmin: int
    tmax: int


def replay_alarm(samples: list[Sample], low: int, high: int, hyst: int,
                 min_duration_s: int) -> list[Interval]:
    """在 PC 侧重放报警状态机，独立验证设备端记录。"""
    out: list[Interval] = []
    state = "NORMAL"
    pending = "NORMAL"
    pending_ts = 0
    start_ts = 0
    tmin = tmax = 0

    for s in samples:
        t = s.t1
        if t == -32768:
            continue

        if state == "NORMAL":
            raw = "HIGH" if t > high else ("LOW" if t < low else "NORMAL")
        elif state == "HIGH":
            raw = "HIGH" if t > high - hyst else "NORMAL"
        else:
            raw = "LOW" if t < low + hyst else "NORMAL"

        if raw == state:
            pending = state
            pending_ts = s.ts
            if state != "NORMAL":
                tmin, tmax = min(tmin, t), max(tmax, t)
            continue

        if raw != pending:
            pending = raw
            pending_ts = s.ts
            continue

        if s.ts - pending_ts < min_duration_s:
            continue

        if state == "NORMAL":
            state = raw
            start_ts = pending_ts
            tmin = tmax = t
        else:
            out.append(Interval(state, start_ts, pending_ts, tmin, tmax))
            state = "NORMAL"
            pending = "NORMAL"
            pending_ts = s.ts

    return out


# ---------------- 渲染 ----------------

def fmt_ts(ts: int) -> str:
    if ts == 0:
        return "—"
    return time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime(ts))


def svg_chart(samples: list[Sample], low: int, high: int,
              width: int = 860, height: int = 260) -> str:
    pts = [(s.ts, s.t1 / 100.0) for s in samples if s.t1 != -32768]
    if len(pts) < 2:
        return "<p>样本不足，无法绘图</p>"

    t0, t1 = pts[0][0], pts[-1][0]
    span = max(t1 - t0, 1)
    lo = min(min(p[1] for p in pts), low / 100.0) - 1
    hi = max(max(p[1] for p in pts), high / 100.0) + 1
    vspan = max(hi - lo, 0.1)

    pad_l, pad_r, pad_t, pad_b = 46, 12, 12, 28
    w = width - pad_l - pad_r
    h = height - pad_t - pad_b

    def x(ts: int) -> float:
        return pad_l + (ts - t0) / span * w

    def y(v: float) -> float:
        return pad_t + (hi - v) / vspan * h

    poly = " ".join(f"{x(t):.1f},{y(v):.1f}" for t, v in pts)

    # Y 轴刻度（5 条）
    grid = []
    for i in range(5):
        v = lo + vspan * i / 4
        yy = y(v)
        grid.append(f'<line x1="{pad_l}" y1="{yy:.1f}" x2="{width-pad_r}" y2="{yy:.1f}" '
                    f'stroke="#e5e7eb" stroke-width="1"/>')
        grid.append(f'<text x="{pad_l-6}" y="{yy+4:.1f}" font-size="10" fill="#6b7280" '
                    f'text-anchor="end">{v:.1f}</text>')

    # 阈值线
    th = []
    for v, color, label in ((low / 100.0, "#f59e0b", "low"),
                            (high / 100.0, "#ef4444", "high")):
        yy = y(v)
        th.append(f'<line x1="{pad_l}" y1="{yy:.1f}" x2="{width-pad_r}" y2="{yy:.1f}" '
                  f'stroke="{color}" stroke-width="1" stroke-dasharray="4 3"/>')
        th.append(f'<text x="{width-pad_r-2}" y="{yy-4:.1f}" font-size="10" '
                  f'fill="{color}" text-anchor="end">{label} {v:.1f}°C</text>')

    return (
        f'<svg viewBox="0 0 {width} {height}" width="100%" role="img" '
        f'aria-label="temperature chart">'
        f'{"".join(grid)}{"".join(th)}'
        f'<polyline points="{poly}" fill="none" stroke="#2563eb" stroke-width="1.6"/>'
        f'<text x="{pad_l}" y="{height-8}" font-size="10" fill="#6b7280">'
        f'{fmt_ts(t0)}</text>'
        f'<text x="{width-pad_r}" y="{height-8}" font-size="10" fill="#6b7280" '
        f'text-anchor="end">{fmt_ts(t1)}</text>'
        f'</svg>'
    )


def render_html(samples: list[Sample], intervals: list[Interval],
                events: list[Event], args) -> str:
    temps = [s.t1 / 100.0 for s in samples if s.t1 != -32768]
    if not temps:
        return "<html><body><p>没有有效样本</p></body></html>"

    t_min, t_max = min(temps), max(temps)
    t_mean = mean(temps)
    t_mkt = mkt(temps)

    span_s = samples[-1].ts - samples[0].ts if len(samples) > 1 else 0
    if len(samples) > 1 and span_s > 0:
        interval_s = span_s / (len(samples) - 1)
    else:
        interval_s = 0

    over = sum(1 for v in temps if v > args.high / 100.0)
    under = sum(1 for v in temps if v < args.low / 100.0)
    excursion_s = sum(iv.end - iv.start for iv in intervals)

    rows = "".join(
        f"<tr><td>{html.escape(iv.kind)}</td><td>{fmt_ts(iv.start)}</td>"
        f"<td>{fmt_ts(iv.end)}</td><td>{iv.end - iv.start}</td>"
        f"<td>{iv.tmin / 100:.2f}</td><td>{iv.tmax / 100:.2f}</td></tr>"
        for iv in intervals
    ) or "<tr><td colspan='6' class='muted'>无超限区间</td></tr>"

    ev_rows = "".join(
        f"<tr><td>{'HIGH' if ev.kind == 1 else 'LOW'}</td>"
        f"<td>{fmt_ts(ev.start)}</td><td>{fmt_ts(ev.end)}</td>"
        f"<td>{ev.end - ev.start}</td><td>{ev.tmin / 100:.2f}</td>"
        f"<td>{ev.tmax / 100:.2f}</td></tr>"
        for ev in events
    ) or "<tr><td colspan='6' class='muted'>无设备端事件</td></tr>"

    chart = svg_chart(samples, args.low, args.high)
    title = html.escape(args.title)

    return f"""<!doctype html>
<html lang="zh"><head><meta charset="utf-8">
<title>{title}</title>
<style>
 body{{font:14px/1.5 system-ui,-apple-system,"Segoe UI",sans-serif;max-width:920px;
       margin:32px auto;padding:0 16px;color:#111827;background:#fff}}
 h1{{font-size:22px;margin:0 0 4px}}
 .sub{{color:#6b7280;margin-bottom:24px}}
 table{{border-collapse:collapse;width:100%;margin:12px 0 24px}}
 th,td{{padding:6px 10px;border-bottom:1px solid #e5e7eb;text-align:left;
        font-variant-numeric:tabular-nums}}
 th{{background:#f9fafb;font-weight:600;font-size:12px;text-transform:uppercase;
     letter-spacing:.03em;color:#374151}}
 .cards{{display:grid;grid-template-columns:repeat(auto-fit,minmax(140px,1fr));
         gap:12px;margin:20px 0}}
 .card{{border:1px solid #e5e7eb;border-radius:8px;padding:12px}}
 .card .k{{font-size:11px;color:#6b7280;text-transform:uppercase;letter-spacing:.04em}}
 .card .v{{font-size:20px;font-weight:600;margin-top:2px}}
 .muted{{color:#9ca3af}}
 .ok{{color:#059669}} .bad{{color:#dc2626}}
 footer{{margin-top:32px;color:#9ca3af;font-size:12px}}
</style></head><body>
<h1>{title}</h1>
<div class="sub">设备时钟 {fmt_ts(samples[0].ts)} 起 · 共 {len(samples)} 条记录 · 采样间隔约 {interval_s:.0f} s</div>

<div class="cards">
  <div class="card"><div class="k">Min</div><div class="v">{t_min:.2f} °C</div></div>
  <div class="card"><div class="k">Max</div><div class="v">{t_max:.2f} °C</div></div>
  <div class="card"><div class="k">Mean</div><div class="v">{t_mean:.2f} °C</div></div>
  <div class="card"><div class="k">MKT</div><div class="v">{t_mkt:.2f} °C</div></div>
  <div class="card"><div class="k">超限时长</div>
       <div class="v {'bad' if excursion_s else 'ok'}">{excursion_s} s</div></div>
</div>

<h2 style="font-size:16px">温度曲线</h2>
{chart}

<h2 style="font-size:16px">阈值判定</h2>
<table>
 <tr><th>项</th><th>值</th></tr>
 <tr><td>下限 / 上限</td><td>{args.low/100:.2f} / {args.high/100:.2f} °C</td></tr>
 <tr><td>回差 / 最短持续</td><td>{args.hyst/100:.2f} °C / {args.min_duration} s</td></tr>
 <tr><td>超上限样本</td><td>{over} ({over*100.0/len(temps):.1f}%)</td></tr>
 <tr><td>超下限样本</td><td>{under} ({under*100.0/len(temps):.1f}%)</td></tr>
 <tr><td>判定</td><td class="{'bad' if intervals else 'ok'}">
   {'不符合' if intervals else '符合'}</td></tr>
</table>

<h2 style="font-size:16px">超限区间（PC 侧重放）</h2>
<table>
 <tr><th>方向</th><th>开始</th><th>结束</th><th>时长(s)</th><th>最低</th><th>最高</th></tr>
 {rows}
</table>

<h2 style="font-size:16px">设备端事件（events 文件）</h2>
<table>
 <tr><th>方向</th><th>开始</th><th>结束</th><th>时长(s)</th><th>最低</th><th>最高</th></tr>
 {ev_rows}
</table>

<footer>
 由 tools/dlreport.py 生成 · 数据来自 {html.escape(str(args.input))} ·
 记录格式 v2（16 字节/条，逐条 CRC16）
</footer>
</body></html>
"""


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="Generate a transport temperature report")
    ap.add_argument("input", help="raw sample file exported by dltool dump")
    ap.add_argument("-o", "--output", default="report.html")
    ap.add_argument("--events", help="optional events file")
    ap.add_argument("--low", type=int, default=200, help="lower limit, x100 °C")
    ap.add_argument("--high", type=int, default=800, help="upper limit, x100 °C")
    ap.add_argument("--hyst", type=int, default=50, help="hysteresis, x100 °C")
    ap.add_argument("--min-duration", type=int, default=60, help="seconds")
    ap.add_argument("--title", default="运输温度报告")
    args = ap.parse_args(argv)

    src = Path(args.input)
    if not src.exists():
        print(f"error: {src} not found", file=sys.stderr)
        return 1

    samples = load_samples(src)
    if not samples:
        print("error: no valid records", file=sys.stderr)
        return 1

    intervals = replay_alarm(samples, args.low, args.high,
                             args.hyst, args.min_duration)
    events = load_events(Path(args.events)) if args.events else []

    out = Path(args.output)
    out.write_text(render_html(samples, intervals, events, args), encoding="utf-8")

    temps = [s.t1 / 100.0 for s in samples if s.t1 != -32768]
    print(f"{len(samples)} records, {len(temps)} valid temperatures")
    print(f"min {min(temps):.2f}  max {max(temps):.2f}  "
          f"mean {mean(temps):.2f}  MKT {mkt(temps):.2f} °C")
    print(f"excursions: {len(intervals)}  device events: {len(events)}")
    print(f"report -> {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
