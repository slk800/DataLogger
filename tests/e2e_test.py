"""端到端测试：dltool 的传输层 + fake_device，不经过真实串口。

Link 走 pyserial 的 loop://；为了让测试可控，这里直接用内存管道
模拟 Link 的读写接口，避免 loop:// 的自我回环问题。
"""
import sys
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))

import dlproto as P            # noqa: E402
import fake_device as FD       # noqa: E402


class MemLink:
    """与 dltool.Link 相同的 request() 接口，但直接连到 FakeDevice。"""

    def __init__(self, dev, timeout=0.5, retries=20):
        self.dev = dev
        self.timeout = timeout
        self.retries = retries
        self._seq = 0
        self._buf = bytearray()

    def _next_seq(self):
        self._seq = (self._seq + 1) & 0xFFFF
        return self._seq

    def request(self, ftype, data=b""):
        seq = self._next_seq()
        pkt = P.Frame(ftype, seq, data).build()
        for _ in range(self.retries):
            self._buf.extend(self.dev.feed(pkt))
            for frame in P.parse_stream(self._buf):
                if frame.seq != seq:
                    continue
                if frame.type == P.T_NAK:
                    raise P.NakError(frame.data[0] if frame.data else 0)
                if frame.type == P.T_ACK:
                    return frame.data
        raise P.ProtoError(f"no response to 0x{ftype:02X}")


def main() -> int:
    failures = 0

    def check(cond, msg):
        nonlocal failures
        print(("ok  : " if cond else "FAIL: ") + msg)
        if not cond:
            failures += 1

    # 丢包率按二项分布：0.6^20 ≈ 3.6e-5，单条请求几乎不会耗尽重传
    for drop in (0.0, 0.3, 0.6):
        fs = FD.FakeFs()
        sample = FD.make_sample_bytes(500)       # 500 * 24 = 12000 bytes
        fs.create("sample", P.FDATA, sample)
        fs.create("log", P.FLOG, b"\x11" * 300)
        dev = FD.FakeDevice(fs, drop_rate=drop, noise_rate=0.05, seed=7)
        link = MemLink(dev)

        hello = P.parse_hello(link.request(P.T_HELLO))
        check(hello["protocol"] == P.VER, f"drop={drop}: HELLO")

        files = P.parse_ls(link.request(P.T_LS))
        check(len(files) == 2, f"drop={drop}: LS returns 2 files")
        check(files[0].name == "sample" and files[0].size == len(sample),
              f"drop={drop}: LS sample size")

        size, dev_crc = P.parse_crc(link.request(P.T_CRC, b"sample".ljust(16, b"\0")))
        check(size == len(sample), f"drop={drop}: CRC size matches")

        # 分块导出
        got = bytearray()
        while len(got) < size:
            want = min(256, size - len(got))
            payload = b"sample".ljust(16, b"\0") + struct.pack("<IH", len(got), want)
            got += link.request(P.T_READ, payload)
        check(bytes(got) == sample, f"drop={drop}: READ reassembles full file")
        check((zlib.crc32(got) & 0xFFFFFFFF) == dev_crc,
              f"drop={drop}: local CRC32 == device CRC32")

        # NAK 路径
        try:
            link.request(P.T_CRC, b"missing".ljust(16, b"\0"))
            check(False, f"drop={drop}: missing file raises NAK")
        except P.NakError as exc:
            check(exc.code == 0x03, f"drop={drop}: missing file -> NO_FILE")

        # STAT
        st = P.parse_stat(link.request(P.T_STAT))
        check(st.files == 2, f"drop={drop}: STAT file count")

        print()

    print(f"e2e_test: {'FAILED' if failures else 'PASSED'} ({failures} failures)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
