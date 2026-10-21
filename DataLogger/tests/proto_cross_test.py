"""C 实现与 Python 实现的线格式交叉验证。

固件侧（C）和上位机侧（Python）必须产生逐字节相同的帧，
否则串口上永远无法通信。这里用固定向量做双向比对。
"""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import dlproto as P  # noqa: E402

C_VECTORS = {
    "hello": None,
    "read_zeros": None,
    "read_max": None,
    "nak": None,
}


def load_c_vectors() -> dict[str, bytes]:
    exe = Path("/tmp/pv")
    if not exe.exists():
        raise SystemExit("build tests/proto_vectors.c first: "
                         "gcc -IProtocol tests/proto_vectors.c Protocol/proto_frame.c -o /tmp/pv")
    out = subprocess.check_output([str(exe)], text=True)
    vec = {}
    for line in out.splitlines():
        name, hexstr = line.split()
        vec[name] = bytes.fromhex(hexstr)
    return vec


def python_vectors() -> dict[str, bytes]:
    payload = bytearray(8)
    payload[3] = 0xAA
    payload[7] = 0x55

    max_payload = bytes(((i + 1) % 256) for i in range(P.MAX_DATA))

    return {
        "hello": P.Frame(P.T_HELLO, 1).build(),
        "read_zeros": P.Frame(P.T_READ, 0x1234, bytes(payload)).build(),
        "read_max": P.Frame(P.T_READ, 0xBEEF, max_payload).build(),
        "nak": P.Frame(P.T_NAK, 0x00FF, bytes([0x03])).build(),   # NO_FILE
    }


def main() -> int:
    c = load_c_vectors()
    py = python_vectors()
    failures = 0

    for name in c:
        if name not in py:
            print(f"FAIL: {name} missing on python side")
            failures += 1
            continue
        if c[name] != py[name]:
            print(f"FAIL: {name} differs")
            print(f"  C : {c[name].hex()[:120]}")
            print(f"  py: {py[name].hex()[:120]}")
            failures += 1
        else:
            print(f"ok  : {name} identical ({len(c[name])} bytes)")

    # 反向：C 生成的帧必须能被 Python 解析
    for name, raw in c.items():
        try:
            frame = P.parse_frame(raw[:-1])
            print(f"ok  : python parses C frame {name} -> type=0x{frame.type:02X} seq={frame.seq}")
        except Exception as exc:  # noqa: BLE001
            print(f"FAIL: python cannot parse C frame {name}: {exc}")
            failures += 1

    # 正向：Python 生成的帧必须能被 C 解析（由 C 侧测试覆盖，这里验证解析对称性）
    for name, raw in py.items():
        frame = P.parse_frame(raw[:-1])
        assert raw.endswith(b"\x00")
        print(f"ok  : python roundtrip {name} -> len={len(frame.data)}")

    print(f"\nproto_cross_test: {'FAILED' if failures else 'PASSED'}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
