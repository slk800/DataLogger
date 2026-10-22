#!/usr/bin/env bash
# 领域层全量测试：C 单测 + 报警状态机 C/Python 交叉 + 报告生成器自检
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CC="${CC:-gcc}"
PY="${PY:-python3}"
fail=0

echo "== 1. domain unit tests =="
$CC -std=c11 -Wall -Wextra -Wno-unused-parameter -I"$ROOT/Domain" \
    "$ROOT/tests/domain_test.c" \
    "$ROOT/Domain/domain_time.c" "$ROOT/Domain/domain_record.c" \
    "$ROOT/Domain/domain_alarm.c" "$ROOT/Domain/domain_mkt.c" \
    -lm -o "$ROOT/tests/.domain_test" || fail=1
[ -x "$ROOT/tests/.domain_test" ] && "$ROOT/tests/.domain_test" || fail=1

echo
echo "== 2. alarm state machine: C vs Python =="
$CC -std=c11 -Wall -Wextra -I"$ROOT/Domain" \
    "$ROOT/tests/alarm_dump.c" "$ROOT/Domain/domain_alarm.c" \
    "$ROOT/Domain/domain_record.c" -o /tmp/alarm_dump || fail=1
(cd "$ROOT" && $PY tests/alarm_cross_test.py) || fail=1

echo
echo "== 3. report generator =="
(cd "$ROOT" && $PY tests/report_test.py) || fail=1

rm -f "$ROOT/tests/.domain_test"
echo
[ "$fail" -eq 0 ] && echo "domain tests: PASSED" || echo "domain tests: FAILED"
exit $fail
