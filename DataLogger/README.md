# DataLogger

面向**冷链 / 便携运输**场景的可靠温度记录仪：STM32F103 + FreeRTOS，
NTC 采集、SSD1306 三键菜单、W25Q64 持久化，支持串口导出、报警事件与
90 天运输报告。

| 项 | 值 |
| --- | --- |
| MCU | STM32F103C8T6（20 KB SRAM，72 MHz） |
| RTOS | FreeRTOS V10.0.1（tickless + STOP） |
| 存储 | W25Q64 8 MB，SPI |
| 传感器 | 10K NTC，B3950，PA0 / ADC1_IN0 |
| 时钟 | PCF8563（I2C，CR2032 保持） |
| 显示 | 0.96" SSD1306，128×64，I2C |
| 已验证构建 | 0 Error / 0 Warning，ROM **55224** B（84.3%），RAM **18760** B（91.6%） |

更完整的接线、菜单和测试见 [系统说明书.md](系统说明书.md)，实现细节见
[技术实现说明.md](技术实现说明.md)，问题记录见 [修复日志.md](修复日志.md)。

---

## 这个项目解决什么问题

一个数据记录器，**数据出不来**就没有意义。本项目围绕三件事构建：

| 问题 | 方案 | 证据 |
| --- | --- | --- |
| 数据怎么拿出来 | COBS 分帧 + CRC16 + 停等重传，PC 端 `verify` 做端到端 CRC 比对 | 60% 随机丢帧下仍能完整还原 |
| 掉电会不会丢数据 | 记录级 CRC + 启动有效前缀修复；配置 A/B 双区 | 四类断电窗口全覆盖 |
| 能不能长期部署 | tickless + STOP + 外设门控 | 模型 54 µA（干净配置） |

## 功能

- **记录格式 v2**：16 字节/条，60 s 采样可存 **90 天**（2.07 MB）
- **报警**：上下限 + 回差 + 最短持续时间，事件写入独立 `events` 文件
- **MKT**：平均动力学温度，冷链/医药行业指标
- **串口导出**：二进制协议与文本 CLI 共用串口，无需专用工具也能排查
- **低功耗**：tickless + STOP，PCF8563 CLKOUT 1 Hz 唤醒
- NTC：**16 次过采样**、去极值平均、**Beta 模型换算**、一阶 **IIR**（7/8 + 1/8）
- 配置 **A/B 双区**：先写 B、验证后再擦写 A，按 magic + CRC + 版本号择新
- W25Q64 上简易文件存储：超级块 / 索引表 / 数据区
- 故障码经 BKP 寄存器跨复位保留（OLED `Flt` / CLI `status`）
- 用 ST-Link 与嘉立创开源 **AixProbe** 做寄存器级实板调试

## 接线

所有模块使用 3.3 V，并与主控共地。NTC 模块的 DO 不连接。

### 基础（W1 之前已具备）

| 模块 | STM32F103 |
| --- | --- |
| NTC AO | PA0（ADC1_IN0） |
| OLED SCL / SDA | PB6 / PB7（I2C1，地址 `0x3C`） |
| W25Q64 CS / SCK / MISO / MOSI | PB0 / PA5 / PA6 / PA7 |
| 上 / 下 / 确认 | PB12 / PB13 / PB14，按下接 GND |
| USART1 TX / RX | PA9 / PA10，115200 8N1 |
| SWDIO / SWCLK | PA13 / PA14 |

### 新增（W2 / W3，全部杜邦线，无需焊接）

| 模块 | STM32F103 | 说明 |
| --- | --- | --- |
| PCF8563 VCC / GND | 3.3V / GND | |
| PCF8563 SDA / SCL | **PB7 / PB6** | 与 OLED 共用 I2C1（地址 `0x51`） |
| PCF8563 **CLKOUT** | **PB1** | 低功耗唤醒源（EXTI1） |
| NTC 模块 #2 AO | **PA1** | ADC1_IN1，需在 CubeMX 中使能 |
| CH340 TX / RX / GND | PA10 / PA9 / GND | **VCC 不接**，防倒灌 |

按键：扫描 50 ms，消抖 30 ms，确认键长按 800 ms，EXTI 下降沿唤醒。

> 完整采购清单与接线图见 [docs/hardware.md](docs/hardware.md)。

## 默认任务

栈大小单位为 32-bit word。

| 任务 | 优先级 | 栈 | 默认 | 职责 |
| --- | ---: | ---: | --- | --- |
| Logger | 3 | 256 | 开 | 从队列取文本日志，写入 W25Q64 |
| DataProd | 2 | 256 | 开* | v1 采集（无 RTC 时使用） |
| Recorder | 2 | 320 | **关** | v2 记录：墙钟、双通道、报警、MKT |
| Display | 2 | 256 | 开 | 按键扫描、六页菜单、OLED 刷新 |
| Proto | 4 | 256 | **开** | 二进制协议 + 文本 CLI |
| Shell | 4 | 256 | 关 | 独立串口 CLI（与 Proto 互斥） |
| Monitor | 1 | 128 | 关 | 串口状态快照 |

\* `DataProd` 与 `Recorder` 互斥：两者都写 `sample` 文件，但记录格式不同
（v1 24 B / v2 16 B），不能混写。

开关在 `App/app_init.h`：

```c
#define APP_ENABLE_PROTO           1   /* 二进制协议 + 文本 CLI */
#define APP_ENABLE_RECORDER        0   /* v2 记录，需 PCF8563 */
#define APP_ENABLE_DATA_PRODUCER   1   /* v1 记录（Recorder 关闭时生效） */
#define APP_ENABLE_UART_CLI        0   /* 独立 Shell，与 Proto 互斥 */
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
| Home | 设备名、温度、样本数、**MKT / 报警状态**、日志丢弃数 |
| System | 堆、运行时间、NTC 状态、ADC、**上次故障、墙钟** |
| Flash | 已用 / 剩余空间、文件数 |
| Log | 日志等级、**报警阈值、事件数**、最近一条文本 |
| Config | 采样周期、日志等级、保存 |
| Action | 保存配置、清除数据、重启 |

`Cnt` 只统计已通过 CRC 并写入 `sample` 文件的条数，不是 ADC 读取次数。

## CLI

打开 `APP_ENABLE_PROTO=1` 时，文本命令仍可用（Proto 任务内部复用 CLI 命令表）。
`APP_ENABLE_UART_CLI=1` 是独立的 Shell 任务，**与 Proto 互斥**（争抢接收缓冲区）。

共 17 条：

```text
help  ls  cat  rm  erase  status  task  heap  set  get  save  loglvl  reset
time  alarm  mkt  i2cscan
```

| 命令 | 说明 |
| --- | --- |
| `time [utc]` | 查看/设置墙钟（写入 PCF8563） |
| `alarm` | 查看报警阈值、状态与事件数 |
| `mkt` | 查看当前 MKT |
| `i2cscan` | 扫描 I2C1，确认 OLED(0x3C) 与 PCF8563(0x51) 是否应答 |

配置键：`sample_ms`、`log_level`、`name`、`t_low`、`t_high`、`t_hyst`、`t_dur`。
`set` 只改内存，需 `save` 才写入 A/B 区。

## 编译

Keil MDK 打开：

```text
MDK-ARM/DataLogger.uvprojx
```

CubeMX 工程：`DataLogger.ioc`。目标芯片 STM32F103C8Tx，下载地址 `0x08000000`。

已验证构建（协议 + v2 记录 + 低功耗全开）：`0 Error(s), 0 Warning(s)`，
ROM **55224** 字节（84.3%），RAM **18760** 字节（91.6%），剩余约 **1.7 KB**。

固件产物（若随仓库发布）：

- `MDK-ARM/DataLogger/DataLogger.hex`
- `MDK-ARM/DataLogger/DataLogger.bin`
- `MDK-ARM/DataLogger/DataLogger.axf`

## 使用

1. 按上表接线，NTC / OLED / W25Q64 / PCF8563 用 3.3 V 并共地。
2. 烧录 hex 或 axf。
3. OLED 进入 Home，上 / 下翻页，确认键操作 Config / Action。
4. 接 CH340 到 PA9/PA10，用上位机工具导出数据（见下文）。

## 硬件采购

完整清单（型号、数量、参考价、接线图）见 [docs/hardware.md](docs/hardware.md)。

**必需（约 ¥30）**：CH340 USB-TTL（3.3V 版）、PCF8563 模块（**需引出 CLKOUT**）、
CR2032、NTC 10K B3950 模块、面包板、杜邦线。

**可选（约 ¥49，功耗实测）**：INA219、USB 功率计、HT7333 模块、18650 电池盒 + 电池。

不需要焊接、不需要晶振、不需要 DS3231。

## 目录

```text
App/        启动与任务（Logger / DataProd / Recorder / Display / Proto）
BSP/        UART、SPI、W25Q64、OLED I2C、按键、PCF8563、低功耗、唤醒源
Sensor/     NTC 采样、B 参数换算、IIR、开路/短路判断
OLED/       SSD1306 帧缓冲、绘制与休眠
UI/         六页菜单状态机与 UI 快照
Log/        文本日志队列、样本 CRC、启动恢复、故障码
FS/         超级块 / 索引表 / 数据区
Config/     A/B 双区配置（含报警阈值）
CLI/        17 条命令
Core/       CubeMX / HAL
Utils/      CRC32 等
MDK-ARM/    Keil 工程
Protocol/   协议帧层（COBS / CRC16 / 帧解析，纯逻辑无 HAL 依赖）
Domain/     时间换算、记录编解码、报警状态机、MKT（纯逻辑，可主机侧测试）
docs/       协议、记录格式、功耗设计、硬件清单
tools/      PC 端工具（dltool / dlreport / fake_device / demo / power_budget）
tests/      主机侧单测、交叉验证与端到端测试
.github/    GitHub Actions（推送即跑五个测试套件）
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

协议规范见 [docs/protocol.md](docs/protocol.md)。

示例报告（合成数据，可直接在 GitHub 上预览曲线）：
[docs/images/demo-report.md](docs/images/demo-report.md)。
简历条目与面试要点见 [docs/resume-bullets.md](docs/resume-bullets.md)。

要点：

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

Keil 侧验证（需要本机安装 MDK）：

```powershell
& "E:\Keil5\Core\UV4\UV4.exe" -b MDK-ARM\DataLogger.uvprojx -j0 -o build.log
# 期望：0 Error(s), 0 Warning(s)
```

## 分支与提交

| 分支 | 内容 |
| --- | --- |
| `main` | 初始版本（W1 之前） |
| `w2-w3` | 技术债清理 + W1/W2/W3 全部改动 |

`w2-w3` 基于独立历史构建，与 `main` **无共同祖先**。合并前建议先对比：

```bash
git fetch datalogger
git diff --stat datalogger/main datalogger/w2-w3
```

提交按功能划分为三个，可单独查看：

```text
0ce1cf0  refactor: 清理技术债，补齐工程规范
8bea9d0  feat: 串口协议、领域层与数据导出工具（W1+W2）
864fd8d  feat: 低功耗改造与运输场景（W3）
```

### 目录结构差异

本地工作目录把项目放在 `DataLogger/` 子目录下，远端 `main` 是根目录结构。
合并时 Git 会显示大量文件"移动"，这是路径差异，不是内容丢失。

### 推送（需要代理）

本机 GitHub 直连不通，系统代理为 `127.0.0.1:7897`，但 git 未配置。推送时显式指定：

```bash
git -c http.proxy=http://127.0.0.1:7897 push datalogger HEAD:refs/heads/w2-w3
```

如需长期生效（仅当前仓库）：

```bash
git config http.proxy http://127.0.0.1:7897
```

### MDK-ARM 子模块

`DataLogger/MDK-ARM` 是 **gitlink**（子模块），但没有 `.gitmodules`，且其自身仓库
与远端 `slk800/MDK-ARM` 已分叉（本地领先 4 / 落后 3）。

该目录里还有 Keil 编译产物（`.o` / `.axf` / `.hex` / `.map` 等约 235 个文件），
**尚未纳入 gitignore**。建议在子模块内单独处理：

```bash
cd DataLogger/MDK-ARM
printf '*.o\n*.d\n*.crf\n*.lnp\n*.sct\n*.htm\n*.dep\n*.map\nDataLogger/\n' >> .gitignore
git rm -r --cached DataLogger/*.o DataLogger/*.d 2>/dev/null
```

在整理干净之前，主仓库的 `DataLogger/MDK-ARM` gitlink 会一直显示为已修改。

## License

[MIT](LICENSE) © 2026 SLK800。STM32 HAL/CMSIS 与 FreeRTOS 按各自原始许可证分发，详见 `Drivers/` 与 `Middlewares/` 下的 LICENSE 文件。

W25Q64 上的是简化文件系统（非 LittleFS，无完整磨损均衡），请自行评估适用边界。
