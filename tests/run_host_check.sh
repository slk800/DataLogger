#!/usr/bin/env bash
# 主机侧编译检查：用 gcc + 桩头文件对所有业务 .c 做 syntax/type 检查。
# 不做链接、不模拟硬件行为，只保证代码在 ARMCC 之外也能通过编译。
#
# 用法：  bash tests/run_host_check.sh
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STUB="$ROOT/tests/hostcheck"
OUT="$(mktemp -d)"
CC="${CC:-gcc}"

INC="-I$STUB -I$ROOT/User -I$ROOT/App -I$ROOT/BSP -I$ROOT/Config -I$ROOT/Log \
     -I$ROOT/FS -I$ROOT/Sensor -I$ROOT/UI -I$ROOT/CLI -I$ROOT/Utils -I$ROOT/OLED -I$ROOT/Protocol -I$ROOT/Domain -I$ROOT/Core/Inc"

SRC="$ROOT/App/app_init.c $ROOT/App/task_data_producer.c $ROOT/App/task_display.c \
     $ROOT/App/task_logger.c $ROOT/App/task_monitor.c $ROOT/App/task_shell.c \
     $ROOT/App/task_recorder.c $ROOT/App/task_idle_hook.c \
     $ROOT/BSP/bsp_flash.c $ROOT/BSP/bsp_spi.c $ROOT/BSP/bsp_uart.c $ROOT/BSP/bsp_key.c \
     $ROOT/BSP/bsp_oled_i2c.c $ROOT/BSP/bsp_rtc.c \
     $ROOT/BSP/bsp_lowpower.c $ROOT/BSP/bsp_wakeup.c \
     $ROOT/Config/config.c $ROOT/Log/log.c $ROOT/Log/sample_record.c $ROOT/Log/fault.c \
     $ROOT/FS/fs_core.c $ROOT/FS/fs_file.c $ROOT/Sensor/sensor_ntc.c \
     $ROOT/UI/ui_menu.c $ROOT/UI/ui_status.c $ROOT/CLI/cli.c $ROOT/CLI/cli_cmd.c \
     $ROOT/Utils/utils.c $ROOT/OLED/ssd1306.c \
     $ROOT/Protocol/proto_frame.c $ROOT/App/proto_cmd.c $ROOT/App/task_proto.c \
     $ROOT/Domain/domain_time.c $ROOT/Domain/domain_record.c \
     $ROOT/Domain/domain_alarm.c $ROOT/Domain/domain_mkt.c"

fails=0
# 主机是 LP64（long 64 位），目标是 ARM ILP32（long 32 位）；
# 工程里 %lu 搭配 uint32_t 在目标平台正确，主机上会误报，故关闭 format 告警。
for f in $SRC; do
    obj="$OUT/$(basename "$f" .c).o"
    if ! $CC -std=c11 -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-format \
             $INC -c "$f" -o "$obj" 2>"$OUT/err.txt"; then
        echo "FAIL: ${f#$ROOT/}"
        sed 's/^/    /' "$OUT/err.txt"
        fails=$((fails+1))
    elif [ -s "$OUT/err.txt" ]; then
        echo "WARN: ${f#$ROOT/}"
        sed 's/^/    /' "$OUT/err.txt"
    fi
done

echo
if [ "$fails" -eq 0 ]; then
    count=$(echo $SRC | wc -w)
    echo "host check: all $count files compiled"
else
    echo "host check: $fails file(s) failed"
fi
rm -rf "$OUT"
exit $fails
