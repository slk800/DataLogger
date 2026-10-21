#!/usr/bin/env bash
# 主机侧运行 Fault 模块行为测试（不需要硬件）
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CC="${CC:-gcc}"

$CC -std=c11 -Wall -Wextra -Wno-unused-parameter \
    -I"$ROOT/tests/hostcheck" -I"$ROOT/Log" \
    "$ROOT/tests/fault_test.c" \
    "$ROOT/Log/fault.c" \
    "$ROOT/tests/hostcheck/hal_stubs.c" \
    -o "$ROOT/tests/.fault_test" || exit 1

"$ROOT/tests/.fault_test"
rc=$?
rm -f "$ROOT/tests/.fault_test"
exit $rc
