#!/usr/bin/env python3
"""DataLogger 功耗预算模型。

用器件数据手册的典型值建立"应该多少电流"的模型，W3 实测时逐项对照。
模型跑不准则说明有漏掉的负载（浮空引脚、模块板载 LDO、上拉电阻等），
这本身就是有价值的诊断结论。

用法：
  python tools/power_budget.py                # 默认 60 s 采样
  python tools/power_budget.py --interval 1   # 1 s 采样
  python tools/power_budget.py --board-led    # 假设未拆板载 LED
  python tools/power_budget.py --no-ldo-swap  # 假设仍用 AMS1117
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass


@dataclass
class Item:
    name: str
    current_ua: float       # 平均电流（µA）
    note: str = ""


def build_model(args) -> tuple[list[Item], dict]:
    """返回 (明细项, 场景参数)。所有电流单位 µA。"""
    items: list[Item] = []

    # ---- 静态（睡眠态）----
    # STM32F103 STOP 模式：典型 20 µA（HSE 关、稳压器低功耗）
    items.append(Item("MCU STOP 静态", 20.0, "RM0008 Table 27"))

    # 板载电源 LED（Blue Pill）：3.3V 经 1k 限流，约 1.5–2.5 mA
    if args.board_led:
        items.append(Item("板载电源 LED", 2000.0, "软件无法关闭，需割线或换板"))

    # LDO 静态电流
    ldo_iq = 4.0 if not args.no_ldo_swap else 4000.0
    items.append(Item("LDO 静态", ldo_iq,
                      "HT7333 (4 µA)" if not args.no_ldo_swap else "AMS1117 (~4 mA)"))

    # 未用引脚漏电：配成模拟输入后可忽略，但 OLED/I2C 上拉仍耗电
    items.append(Item("I2C 上拉（空闲）", 0.0, "SCL/SDA 高电平时不耗电"))

    # PCF8563 计时（模块自带电池时由电池供电，这里按 0 计）
    items.append(Item("PCF8563 计时", 0.0, "CR2032 供电" if args.rtc_battery else 0.25))

    # NTC 分压（GPIO 门控，测量时才有电流）
    # 3.3V / (10k + 10k) ≈ 165 µA，但占空比极小
    ntc_active_ua = 165.0

    # W25Q64 深睡 1 µA；待机 4 mA（仅在写入时短暂处于待机）
    items.append(Item("W25Q64 深睡", 1.0, "0xB9 指令后"))

    # 传感器模块（若用模块而非裸 NTC）
    sensor_idle = 1000.0 if args.sensor_module else 0.0
    if sensor_idle:
        items.append(Item("NTC 模块常驻", sensor_idle, "LM393 + LED，改用裸 NTC 可消除"))

    # ---- 动态（每次唤醒）----
    wake_ms = args.wake_ms
    active_ma = args.active_ma

    # 每秒唤醒一次（CLKOUT 1 Hz）
    wake_duty = wake_ms / 1000.0
    wake_avg_ua = active_ma * 1000.0 * wake_duty
    items.append(Item("每秒唤醒（EXTI）", wake_avg_ua,
                      f"{wake_ms} ms @ {active_ma} mA"))

    # 每次采样：NTC 测量 + ADC + Flash 写入
    sample_ms = args.sample_ms
    sample_duty = sample_ms / 1000.0
    sample_avg_ua = active_ma * 1000.0 * sample_duty / args.interval
    items.append(Item("每周期采样", sample_avg_ua,
                      f"{sample_ms} ms / {args.interval} s"))

    # Flash 写入：每 N 条批量提交一次，每批 2 次扇区擦除（索引 + 超级块）
    # 扇区擦除 45 ms @ 15 mA
    erase_ms = 45.0
    erases_per_sample = 2.0 / max(args.batch, 1)
    flash_avg_ua = 15000.0 * (erase_ms / 1000.0) * erases_per_sample / args.interval
    items.append(Item("Flash 擦写", flash_avg_ua,
                      f"每 {args.batch} 条提交，2 次擦除/批"))

    # OLED：事件驱动，只在有变化时刷新
    # 刷新一次：72 页 × 1 KB I2C @ 400 kHz ≈ 30 ms @ 15 mA
    oled_refresh_ms = 30.0
    oled_per_hour = args.oled_refreshes_per_hour
    oled_avg_ua = 15000.0 * (oled_refresh_ms / 1000.0) * oled_per_hour / 3600.0
    items.append(Item("OLED 刷新", oled_avg_ua,
                      f"{oled_per_hour} 次/小时，{oled_refresh_ms} ms/次"))

    # 睡眠时 OLED 电荷泵关闭
    items.append(Item("OLED 休眠", 10.0, "0xAE + 关电荷泵"))

    ctx = {
        "interval": args.interval,
        "battery_mah": args.battery_mah,
    }
    return items, ctx


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="DataLogger power budget model")
    ap.add_argument("--interval", type=float, default=60.0, help="sampling interval (s)")
    ap.add_argument("--battery-mah", type=float, default=1000.0)
    ap.add_argument("--board-led", action="store_true", help="assume stock Blue Pill LED")
    ap.add_argument("--no-ldo-swap", action="store_true", help="assume AMS1117")
    ap.add_argument("--sensor-module", action="store_true", help="assume NTC module with LM393")
    ap.add_argument("--rtc-battery", action="store_true", default=True)
    ap.add_argument("--no-rtc-battery", dest="rtc_battery", action="store_false")
    ap.add_argument("--wake-ms", type=float, default=0.3, help="ms awake per 1 Hz wake")
    ap.add_argument("--active-ma", type=float, default=15.0, help="active current at 72 MHz")
    ap.add_argument("--sample-ms", type=float, default=25.0, help="ms active per sample")
    ap.add_argument("--batch", type=int, default=32, help="records per Flash commit")
    ap.add_argument("--oled-refreshes-per-hour", type=float, default=60.0)
    args = ap.parse_args(argv)

    items, ctx = build_model(args)
    total_ua = sum(i.current_ua for i in items)

    print(f"采样间隔 {ctx['interval']:.0f} s · 电池 {ctx['battery_mah']:.0f} mAh")
    print()
    print(f"{'项目':<26} {'电流 (µA)':>12}   说明")
    print("-" * 74)
    for it in sorted(items, key=lambda x: -x.current_ua):
        if it.current_ua > 0:
            print(f"{it.name:<26} {it.current_ua:>12.1f}   {it.note}")
    print("-" * 74)
    print(f"{'合计':<26} {total_ua:>12.1f}")

    ma = total_ua / 1000.0
    hours = ctx["battery_mah"] / ma if ma > 0 else float("inf")
    print()
    print(f"平均电流 : {ma:.3f} mA")
    print(f"预计续航 : {hours:.0f} 小时 ≈ {hours/24:.0f} 天")

    if args.board_led or args.no_ldo_swap or args.sensor_module:
        print()
        print("提示：当前配置包含板级/模块级固定开销，"
              "软件优化无法消除，报告里应单独列出。")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
