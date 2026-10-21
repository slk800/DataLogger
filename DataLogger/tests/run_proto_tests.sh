#!/usr/bin/env bash
# 协议层全量测试：C 单测 + C/Python 交叉验证 + 假设备端到端（需要 pyserial）
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CC="${CC:-gcc}"
PY="${PY:-python3}"
fail=0

echo "== 1. C frame tests =="
$CC -std=c11 -Wall -Wextra -Wno-unused-parameter -I"$ROOT/Protocol" \
    "$ROOT/tests/proto_test.c" "$ROOT/Protocol/proto_frame.c" \
    -o "$ROOT/tests/.proto_test" || fail=1
[ -x "$ROOT/tests/.proto_test" ] && "$ROOT/tests/.proto_test" || fail=1

echo
echo "== 2. C/Python wire-format cross check =="
$CC -std=c11 -I"$ROOT/Protocol" "$ROOT/tests/proto_vectors.c" \
    "$ROOT/Protocol/proto_frame.c" -o /tmp/pv || fail=1
(cd "$ROOT" && $PY tests/proto_cross_test.py) || fail=1

echo
echo "== 3. end-to-end against fake device =="
(cd "$ROOT" && $PY tests/e2e_test.py) || fail=1

rm -f "$ROOT/tests/.proto_test"
echo
[ "$fail" -eq 0 ] && echo "proto tests: PASSED" || echo "proto tests: FAILED"
exit $fail
