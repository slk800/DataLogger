"""报告生成器自检：合成数据 -> 报告 -> 校验关键数字。"""
import re
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import dlreport as R  # noqa: E402


def make_records(temps: list[int], base_ts: int = 1792000000, step: int = 60) -> bytes:
    out = bytearray()
    for i, t in enumerate(temps):
        body = struct.pack("<IhhHBBH", base_ts + i * step, t, -32768, 3300, 0, i & 0xFF, 0)
        crc = R.crc16(body)
        out += body + struct.pack("<H", crc)
    return bytes(out)


def main() -> int:
    failures = 0

    def check(cond, msg):
        nonlocal failures
        print(("ok  : " if cond else "FAIL: ") + msg)
        if not cond:
            failures += 1

    with tempfile.TemporaryDirectory() as td:
        td = Path(td)

        # 1. 恒定温度：MKT == 温度，无超限
        raw = make_records([500] * 100)
        (td / "flat.bin").write_bytes(raw)
        rc = subprocess.run([sys.executable, str(ROOT / "tools/dlreport.py"),
                             str(td / "flat.bin"), "-o", str(td / "flat.html"),
                             "--low", "200", "--high", "800"],
                            capture_output=True, text=True)
        check(rc.returncode == 0, "flat report generated")
        h = (td / "flat.html").read_text(encoding="utf-8")
        check("MKT</div><div class=\"v\">5.00" in h, "constant temp -> MKT 5.00")
        check("无超限区间" in h, "no excursions for flat data")
        check("<svg" in h, "SVG chart embedded")

        # 2. 一段高温：应产生 1 个 HIGH 区间
        temps = [500] * 50 + [950] * 50 + [500] * 50
        (td / "spike.bin").write_bytes(make_records(temps))
        rc = subprocess.run([sys.executable, str(ROOT / "tools/dlreport.py"),
                             str(td / "spike.bin"), "-o", str(td / "spike.html"),
                             "--low", "200", "--high", "800", "--min-duration", "60"],
                            capture_output=True, text=True)
        check(rc.returncode == 0, "spike report generated")
        h = (td / "spike.html").read_text(encoding="utf-8")
        check(h.count("<tr><td>HIGH</td>") >= 1, "HIGH interval detected")
        check("不符合" in h, "verdict is non-compliant")

        # 3. 坏 CRC 记录应被跳过
        bad = bytearray(make_records([500] * 20))
        bad[3] ^= 0xFF
        (td / "bad.bin").write_bytes(bytes(bad))
        rc = subprocess.run([sys.executable, str(ROOT / "tools/dlreport.py"),
                             str(td / "bad.bin"), "-o", str(td / "bad.html")],
                            capture_output=True, text=True)
        check("skipped 1 record" in rc.stderr, "corrupted record skipped")

        # 4. 空文件
        (td / "empty.bin").write_bytes(b"")
        rc = subprocess.run([sys.executable, str(ROOT / "tools/dlreport.py"),
                             str(td / "empty.bin"), "-o", str(td / "e.html")],
                            capture_output=True, text=True)
        check(rc.returncode == 1, "empty file rejected")

    print(f"\nreport_test: {'FAILED' if failures else 'PASSED'}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
