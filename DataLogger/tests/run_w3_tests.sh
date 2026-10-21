#!/usr/bin/env bash
# W3 软件部分：功耗模型 + 无硬件演示冒烟
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PY="${PY:-python3}"
fail=0

echo "== 1. power budget model =="
$PY "$ROOT/tests/power_budget_test.py" || fail=1

echo
echo "== 2. hardware-free demo =="
DEMO="$ROOT/build/demo"
rm -rf "$DEMO"
$PY "$ROOT/tools/demo.py" --out "$DEMO" || fail=1
for f in sample.bin events.bin report.html; do
    if [ -s "$DEMO/$f" ]; then
        echo "ok  : produced $f ($(stat -c%s "$DEMO/$f") bytes)"
    else
        echo "FAIL: missing $f"
        fail=1
    fi
done
grep -q "<svg" "$DEMO/report.html" && echo "ok  : report contains SVG chart" || { echo "FAIL: no SVG"; fail=1; }

echo
[ "$fail" -eq 0 ] && echo "w3 tests: PASSED" || echo "w3 tests: FAILED"
exit $fail
