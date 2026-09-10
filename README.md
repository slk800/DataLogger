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

配置键：`sample_ms`、`log_level`、`max_log`、`name`。`set` 只改内存，需 `save` 才写入 A/B 区。

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
```

`Core/Src/freertos.c` 里 CubeMX 生成的 `defaultTask`（Flash/CRC 试验代码）**不会启动**：`main()` 只调用 `App_Init()`，由其中的 `vTaskStartScheduler()` 接管。

## License

本仓库未指定许可证。引用或二次开发请保留作者信息，并自行评估 W25Q64 简易文件存储的适用边界（非 LittleFS，无完整磨损均衡）。
