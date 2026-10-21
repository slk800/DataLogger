# DataLogger

基于 **STM32F103C8T6 + FreeRTOS** 的本地温度采集与可靠日志系统。

真实 **NTC** 采集、**SSD1306 OLED 三键菜单**、**W25Q64** 持久化存储。默认跑采集 / 日志 / 显示 3 个任务；串口 CLI 可编译打开，日常使用不需要 USB 转串口。

| 项 | 值 |
| --- | --- |
| MCU | STM32F103C8T6（20 KB SRAM） |
| RTOS | FreeRTOS V10.0.1 |
| 存储 | W25Q64 8 MB，SPI |
| 传感器 | 10K NTC，B3950，PA0 / ADC1_IN0 |
| 显示 | 0.96" SSD1306，128×64，I2C |
| 已验证构建 | 0 Error / 0 Warning，ROM 39488 B，RAM 16440 B |

更完整的接线、菜单和测试见 [系统说明书.md](系统说明书.md)，实现细节见 [技术实现说明.md](技术实现说明.md)，问题记录见 [修复日志.md](修复日志.md)。

## 功能

- 默认 **3 个应用任务**：DataProd（采集）、Logger（Flash 日志）、Display（OLED + 按键）
- **13 条串口 CLI**（`help` / `status` / `save` / `cat` 等）和 Monitor 任务默认关闭，改宏后可打开
- NTC：**16 次过采样**、去极值平均、**Beta 模型换算**、一阶 **IIR**（7/8 + 1/8）
- 样本逐条 **CRC32**；启动时扫描有效前缀并修复被复位打断的文件元数据
- 配置 **A/B 双区**：先写 B、验证后再擦写 A，按 magic + CRC + 版本号择新
- W25Q64 上简易文件存储：超级块 / 索引表 / 数据区，支持读写、删除、擦除
- 用 ST-Link 与嘉立创开源 **AixProbe** 做寄存器级实板调试

## 接线

所有模块使用 3.3 V，并与主控共地。NTC 模块的 DO 不连接。

| 模块 | STM32F103 |
| --- | --- |
| NTC AO | PA0（ADC1_IN0） |
| OLED SCL / SDA | PB6 / PB7（I2C1，地址 `0x3C`） |
| W25Q64 CS / SCK / MISO / MOSI | PB0 / PA5 / PA6 / PA7 |
| 上 / 下 / 确认 | PB12 / PB13 / PB14，按下接 GND |
| USART1 TX / RX（可选） | PA9 / PA10，115200 8N1 |
| SWDIO / SWCLK | PA13 / PA14 |

按键：扫描 50 ms，消抖 30 ms，确认键长按 800 ms。

## 默认任务

栈大小单位为 32-bit word。

| 任务 | 优先级 | 栈 | 默认 | 职责 |
| --- | ---: | ---: | --- | --- |
| Logger | 3 | 256 | 开 | 从队列取文本日志，写入 W25Q64 |
| DataProd | 2 | 256 | 开 | NTC 采样、样本追加、投递日志 |
| Display | 2 | 256 | 开 | 按键扫描、六页菜单、OLED 刷新 |
| Shell | 4 | 256 | **关** | 串口 CLI |
| Monitor | 1 | 128 | **关** | 串口状态快照 |

打开 CLI / Monitor：在 `App/app_init.h` 将下面两个宏改为 `1` 后重新编译。

```c
#define APP_ENABLE_UART_CLI        0
#define APP_ENABLE_UART_MONITOR    0
```

IPC：日志队列深度 16；Flash 操作由互斥锁保护；UI 状态经互斥锁做快照后再刷新 OLED。

## 配置掉电：四类窗口

`Config_Save()` 顺序为：**写 B → 验证 B → 擦 A → 写 A → 验证 A**。启动时 `Config_Load()` 校验 magic + CRC，双区都有效则选版本号更高的，单区有效则用它并修复另一区。

| 断电时刻 | 结果 |
| --- | --- |
| 写 B 写到一半 | A 仍是上一份有效配置 |
| B 已写完、尚未擦 A | A、B 都有效，按版本号选 B |
| 已擦 A、尚未写完 A | A 无效，B 有效，启动后用 B 并修复 A |
| 写 A 写到一半 | A 校验失败，B 完整，启动后选 B |

样本文件另外做逐记录 CRC：复位落在「数据已写、文件 CRC 未提交」窗口时，启动扫描有效前缀并 `RepairPrefix`。

## OLED 菜单

上电进入 Home。上 / 下浏览页面，确认短按进入或执行，确认长按返回 Home；清除数据需在确认页长按。

```text
Home -> System -> Flash -> Log -> Config
                              -> Action
```

| 页面 | 内容 |
| --- | --- |
| Home | 设备名、温度、有效样本数、Tick、日志丢弃数 |
| System | 堆、运行时间、NTC 状态、ADC |
| Flash | 已用 / 剩余空间、文件数 |
| Log | 日志等级、丢弃数、最近一条文本 |
| Config | 采样周期、日志等级、保存 |
| Action | 保存配置、清除数据、重启 |

`Cnt` 只统计已通过 CRC 并写入 `sample` 文件的条数，不是 ADC 读取次数。

## 可选 CLI

仅在 `APP_ENABLE_UART_CLI=1` 时可用。共 13 条：

```text
help  ls  cat  rm  erase  status  task  heap  set  get  save  loglvl  reset
```

配置键：`sample_ms`、`log_level`、`name`。`set` 只改内存，需 `save` 才写入 A/B 区。

## 编译

Keil MDK 打开：

```text
MDK-ARM/DataLogger.uvprojx
```

CubeMX 工程：`DataLogger.ioc`。目标芯片 STM32F103C8Tx，下载地址 `0x08000000`。

已验证构建：`0 Error(s), 0 Warning(s)`，ROM **39488** 字节，RAM **16440** 字节。

固件产物（若随仓库发布）：

- `MDK-ARM/DataLogger/DataLogger.hex`
- `MDK-ARM/DataLogger/DataLogger.bin`
- `MDK-ARM/DataLogger/DataLogger.axf`

## 使用

1. 按上表接线，NTC / OLED / W25Q64 用 3.3 V 并共地。
2. 烧录 hex 或 axf。
3. OLED 进入 Home，上 / 下翻页，确认键操作 Config / Action。
4. 需要串口调试时打开 `APP_ENABLE_UART_CLI`，115200 8N1。

## 目录

```text
App/        启动与任务（Logger / DataProd / Display，可选 Shell、Monitor）
BSP/        UART、SPI、W25Q64、OLED I2C、按键
Sensor/     NTC 采样、B 参数换算、IIR、开路/短路判断
OLED/       SSD1306 帧缓冲与绘制
UI/         六页菜单状态机与 UI 快照
Log/        文本日志队列、样本 CRC、启动恢复
FS/         超级块 / 索引表 / 数据区
Config/     A/B 双区配置
CLI/        可选 13 条命令
Core/       CubeMX / HAL
Utils/      CRC32 等
MDK-ARM/    Keil 工程
Protocol/   协议帧层（COBS / CRC16 / 帧解析，纯逻辑无 HAL 依赖）
Domain/     时间换算、记录编解码、报警状态机、MKT（纯逻辑，可主机侧测试）
docs/       协议、记录格式、功耗设计
tools/      PC 端工具（dltool / fake_device / dlproto）
tests/      主机侧单测与端到端测试
```

`Core/Src/freertos.c` 只保留内核钩子与静态内存回调；CubeMX 自带的 `defaultTask` 及其实验代码已移除。所有应用任务由 `App_Init()` 创建，`main()` 之后由 `vTaskStartScheduler()` 接管。

## 低功耗（可选）

打开 `APP_ENABLE_RECORDER` 且检测到 PCF8563 时，系统自动启用 tickless + STOP：

```text
空闲 → 关 SysTick → OLED 休眠 + Flash 深睡 → STOP
     ← EXTI（PCF8563 CLKOUT 1 Hz）或按键唤醒
```

| 改动 | 效果 |
| --- | --- |
| `configUSE_TICKLESS_IDLE = 2` + 自定义 `vPortSuppressTicksAndSleep` | 空闲时 CPU 停在 STOP（20 µA） |
| 按键改 EXTI 唤醒 | 消除 50 ms 轮询（约 1.4 mA） |
| OLED 事件驱动 + 空闲降频 | 常态不刷新 |
| OLED 关电荷泵、W25Q64 深睡 | 8 mA → 10 µA、4 mA → 1 µA |
| `vTaskDelayUntil` 固定采样周期 | 周期不受 Flash 写入影响 |

功耗预算模型（`tools/power_budget.py`，按数据手册值）：

```text
60 s 采样 · 干净配置（拆 LED + HT7333 + 裸 NTC）   54 µA  ≈ 772 天（1000 mAh）
60 s 采样 · 原版 Blue Pill                        7.05 mA ≈ 6 天
```

差距来自**板级固定开销**（电源 LED 2 mA + AMS1117 4 mA + NTC 模块 1 mA），
软件无法消除。完整分析见 [docs/power.md](docs/power.md)。

> W3 实测数字待硬件到位后填入 docs/power.md §5。

## 记录格式 v2 与运输场景（可选）

打开 `APP_ENABLE_RECORDER` 后启用 v2 记录任务（需 PCF8563）：

```c
#define APP_ENABLE_RECORDER        1   /* 与 APP_ENABLE_DATA_PRODUCER 互斥 */
```

| 能力 | 说明 |
| --- | --- |
| 墙钟时间戳 | PCF8563（I2C1，与 OLED 共用），记录写入 UTC 秒 |
| 16 字节记录 | 60 s 采样可存 **90 天**（129600 条 × 16 B ≈ 2.07 MB） |
| 双通道 | CH1 真实 NTC；CH2 预留（未接时记 INVALID） |
| 报警 | 上下限 + 回差 + 最短持续时间，事件写入独立 `events` 文件 |
| MKT | 平均动力学温度，冷链/医药行业指标 |
| 时钟丢失标记 | PCF8563 的 VL 位，记录 flags bit3 |

报警配置（单位 0.01 °C，可用 `set` / 协议写入）：

```text
t_low=200  t_high=800  t_hyst=50  t_dur=60
```

生成运输报告（内嵌 SVG，无需 matplotlib）：

```bash
python tools/dltool.py --port COM3 verify --name sample -o sample.bin
python tools/dltool.py --port COM3 events --dump -o events.bin
python tools/dlreport.py sample.bin --events events.bin -o report.html
```

报告包含 min/max/mean/MKT、超限区间（PC 侧独立重放状态机）、设备端事件表。

## 数据导出（可选）

打开 `App/app_init.h` 的 `APP_ENABLE_PROTO` 后，同一串口同时支持**文本 CLI** 和**二进制协议**：

```c
#define APP_ENABLE_PROTO           1
```

上位机工具（无需额外安装，除 `pyserial` 外无依赖）：

```bash
pip install pyserial

python tools/dltool.py --port COM3 info            # 握手 + 存储状态
python tools/dltool.py --port COM3 ls              # 列出文件
python tools/dltool.py --port COM3 verify --name sample -o sample.bin
python tools/dltool.py --port COM3 plot sample.bin # 生成 CSV（装 matplotlib 则同时出图）
```

协议规范见 [docs/protocol.md](docs/protocol.md)。要点：

- COBS 分帧 + CRC16/CCITT-FALSE + 序号停等重传
- `verify` 会比对设备端文件级 CRC32 与 PC 端重算值，不一致即判定导出失败
- `dump --resume` 支持断点续传

开启协议 + v2 记录 + 低功耗后资源占用：**ROM 54768 B（83.6%）、RAM 18456 B（90.1%）**，0 Error / 0 Warning。

## 故障诊断

栈溢出或堆申请失败时，钩子只写 volatile 变量与 BKP 备份寄存器，不调用 UART、队列或互斥量，避免二次故障掩盖根因。故障码与任务名在复位后仍可读：

- OLED `System` 页显示 `Flt`（上次故障）与 `Boot`（启动次数）
- CLI `status` 输出 `LastFault`、`Boot` 与本次的 `RCC_CSR` 复位标志

故障码：`0` 正常、`1` 栈溢出、`2` 堆申请失败。

## 测试

不需要硬件：

```bash
bash tests/run_host_check.sh      # gcc + 桩头文件，编译检查全部业务源码
bash tests/run_fault_test.sh      # 故障码跨复位保留行为
bash tests/run_proto_tests.sh     # 协议：C 单测 + C/Python 线格式交叉 + 假设备端到端
bash tests/run_domain_tests.sh    # 领域层：时间/记录/报警/MKT + 报警状态机 C/Python 交叉 + 报告
bash tests/run_w3_tests.sh        # 功耗预算模型 + 无硬件演示（导出样本/事件/生成报告）

# 无硬件演示（生成 sample.bin / events.bin / report.html）
python tools/demo.py --out demo/
# 功耗预算
python tools/power_budget.py --interval 60
python tools/power_budget.py --interval 60 --board-led --no-ldo-swap --sensor-module
```

`run_proto_tests.sh` 的端到端部分会把丢包率调到 60%、并注入垃圾字节，
用来验证重传与流重同步。

## License

[MIT](LICENSE) © 2026 SLK800。STM32 HAL/CMSIS 与 FreeRTOS 按各自原始许可证分发，详见 `Drivers/` 与 `Middlewares/` 下的 LICENSE 文件。

W25Q64 上的是简化文件系统（非 LittleFS，无完整磨损均衡），请自行评估适用边界。
