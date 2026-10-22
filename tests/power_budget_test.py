"""功耗预算模型自检。

模型的价值在于"可对照"：如果实测与模型差一个数量级，
说明有模型未覆盖的负载，这正是诊断的起点。
"""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def run(*extra: str) -> str:
    return subprocess.check_output(
        [sys.executable, str(ROOT / "tools/power_budget.py"), *extra],
        text=True, encoding="utf-8")


def main() -> int:
    failures = 0

    def check(cond, msg):
        nonlocal failures
        print(("ok  : " if cond else "FAIL: ") + msg)
        if not cond:
            failures += 1

    base = run("--interval", "60")
    check("平均电流 : 0.0" in base, "clean build is sub-100 µA")
    check("701" in base or "772" in base or "18535" in base,
          "clean build reaches hundreds of days")

    board = run("--interval", "60", "--board-led", "--no-ldo-swap", "--sensor-module")
    check("7.05" in board, "stock board model is ~7 mA")
    check("板载电源 LED" in board, "board LED itemized")
    check("AMS1117" in board, "AMS1117 itemized")

    # 采样间隔越短，平均电流越高
    fast = run("--interval", "1")
    slow = run("--interval", "300")
    def avg(text: str) -> float:
        for line in text.splitlines():
            if line.startswith("平均电流"):
                return float(line.split(":")[1].split()[0])
        return -1
    check(avg(fast) > avg(slow), "shorter interval draws more current")

    print(f"\npower_budget_test: {'FAILED' if failures else 'PASSED'}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
