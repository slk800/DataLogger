"""报警状态机 C 实现与 Python 重放的交叉验证。

dlreport.replay_alarm 是 PC 侧独立实现；如果两者对同一序列给出不同的
区间，说明至少一方有 bug —— 这正是 W2 想要的验证。
"""
import random
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import dlreport as R  # noqa: E402


def gen_sequence(seed: int, n: int) -> list[tuple[int, int]]:
    rng = random.Random(seed)
    out = []
    t = 5.0
    for i in range(n):
        t += rng.uniform(-0.4, 0.4)
        t = max(-10.0, min(20.0, t))
        # 周期性制造越限
        if 20 <= (i % 200) < 60:
            t += 3.0
        if 120 <= (i % 200) < 140:
            t -= 6.0
        out.append((1792000000 + i * 60, int(t * 100)))
    return out


def run_c(seq, low, high, hyst, dur) -> list[tuple]:
    exe = Path("/tmp/alarm_dump")
    if not exe.exists():
        raise SystemExit("build tests/alarm_dump.c first")
    text = "\n".join(f"{ts} {t}" for ts, t in seq) + "\n"
    out = subprocess.check_output(
        [str(exe), str(low), str(high), str(hyst), str(dur)],
        input=text, text=True)
    rows = []
    for line in out.strip().splitlines():
        kind, start, end, tmin, tmax = line.split()
        rows.append((kind, int(start), int(end), int(tmin), int(tmax)))
    return rows


def run_py(seq, low, high, hyst, dur) -> list[tuple]:
    samples = [R.Sample(ts, t, -32768, 0, 0, 0, 0) for ts, t in seq]
    ivs = R.replay_alarm(samples, low, high, hyst, dur)
    return [(iv.kind, iv.start, iv.end, iv.tmin, iv.tmax) for iv in ivs]


def main() -> int:
    failures = 0
    for seed in (1, 2, 3):
        seq = gen_sequence(seed, 2000)
        for low, high, hyst, dur in ((200, 800, 50, 60),
                                     (300, 700, 100, 120),
                                     (100, 900, 20, 30)):
            c = run_c(seq, low, high, hyst, dur)
            p = run_py(seq, low, high, hyst, dur)
            if c == p:
                print(f"ok  : seed={seed} limits={low}..{high} -> {len(c)} intervals")
            else:
                print(f"FAIL: seed={seed} limits={low}..{high}")
                print(f"  C : {c[:4]}")
                print(f"  py: {p[:4]}")
                failures += 1

    print(f"\nalarm_cross_test: {'FAILED' if failures else 'PASSED'}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
