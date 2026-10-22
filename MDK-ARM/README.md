# DataLogger 项目说明与测试手册

**固件版本**: v1.0.0
**MCU型号**: STM32F103C8T6
**Flash**: W25Q64 (8MB)
**RTOS**: FreeRTOS V10.0.1 (CMSIS_V1)
**更新日期**: 2026-05-01

---

## 1. 项目概述

DataLogger 是一个基于 STM32F103 + W25Q64 的嵌入式数据采集与存储系统。支持多任务并行运行（传感器数据产生、日志写入 Flash、串口 CLI 交互、系统监控），配置文件支持断电保存与双区冗余恢复。

## 2. 目录结构

```
DataLogger/
├── App/            # 应用层：任务初始化、任务实现
├── BSP/            # 板级支持包：UART、SPI、Flash 驱动
├── CLI/            # 命令行系统：命令解析、13条内建命令
├── Config/         # 配置模块：双区冗余、断电保护
├── Core/           # STM32CubeMX 生成：HAL 初始化、中断向量表
├── Doc/            # 项目文档
├── Drivers/        # STM32F1 HAL 库
├── FS/             # 文件系统：超级块、索引表、文件读写
├── Log/            # 日志模块：记录结构体、等级过滤、队列发送
├── Middlewares/    # FreeRTOS 内核
├── MDK-ARM/        # Keil MDK-ARM 工程文件
├── User/           # global.h 全局头文件
└── Utils/          # 工具函数：CRC32、字符串转换
```

## 3. 模块架构

```
┌──────────────────────────────────────────────────────┐
│                    用户 (串口终端)                     │
└─────────────────────┬────────────────────────────────┘
                      │ USART1 (115200-8-N-1)
┌─────────────────────▼────────────────────────────────┐
│  Task_Shell (pri 4)    CLI 解析与命令执行              │
│  Task_Monitor (pri 1)  系统快照                        │
│  Task_DataProducer (pri 2)  模拟传感器数据              │
│  Task_Logger (pri 3)   日志写入 Flash                  │
├──────────────────────────────────────────────────────┤
│  IPC: g_LogQueue(16) / g_FlashMutex / g_SysEvents     │
├──────────────────────────────────────────────────────┤
│  Log 模块  │  Config 模块  │  FS 文件系统               │
├──────────────────────────────────────────────────────┤
│  BSP: UART / SPI / Flash (W25Q64)                     │
│  Utils: CRC32 / StrToInt / StrTrim / StrSplit          │
└──────────────────────────────────────────────────────┘
```

## 4. 引脚分配

| 引脚 | 功能 | 状态 |
|------|------|------|
| PA5 | SPI1_SCK (W25Q64 时钟) | 已验证 |
| PA6 | SPI1_MISO (W25Q64 数据输入) | 已验证 |
| PA7 | SPI1_MOSI (W25Q64 数据输出) | 已验证 |
| PB0 | W25Q_CS (片选) | 已验证 |
| PA9 | USART1_TX (串口发送) | 已验证 |
| PA10 | USART1_RX (串口接收) | 已验证 |
| PC13 | LED (状态指示灯) | 已验证 |

## 5. 任务配置

| 任务 | 优先级 | 栈大小 | 周期 | 功能 |
|------|--------|--------|------|------|
| Monitor | 1 | 128 words | 5s | 输出系统快照 |
| DataProducer | 2 | 128 words | 可配置 | 产生模拟传感器日志 |
| Logger | 3 | 256 words | 事件驱动 | 将日志队列写入 Flash |
| Shell | 4 | 256 words | 10ms轮询 | CLI 交互 |

## 6. CLI 命令参考

### 6.1 系统命令

| 命令 | 参数 | 说明 | 示例 |
|------|------|------|------|
| `help` | 无 | 显示所有命令 | `help` |
| `task` | 无 | FreeRTOS 任务列表 | `task` |
| `heap` | 无 | 堆内存使用情况 | `heap` |
| `status` | 无 | Flash 使用量与文件数 | `status` |
| `reset` | 无 | 系统复位 | `reset` |

### 6.2 文件命令

| 命令 | 参数 | 说明 | 示例 |
|------|------|------|------|
| `ls` | 无 | 列出所有文件 | `ls` |
| `cat` | `<name>` | 读取文件内容 | `cat log` |
| `rm` | `<name>` | 删除文件 | `rm old_log` |
| `erase` | 无 | 擦除全部数据区 | `erase` |

### 6.3 配置命令

| 命令 | 参数 | 说明 | 示例 |
|------|------|------|------|
| `set` | `<key> <val>` | 修改配置项 | `set sample_ms 500` |
| `get` | `<key>` | 读取配置项 | `get sample_ms` |
| `save` | 无 | 保存配置到 Flash | `save` |
| `loglvl` | `<0-3>` | 设置日志等级 | `loglvl 3` |

### 6.4 Config Key 一览

| Key | 类型 | 默认值 | 说明 |
|-----|------|--------|------|
| `sample_ms` | 整数 | 1000 | DataProducer 采样间隔 (ms) |
| `log_level` | 0-3 | 1 | 0=DEBUG,1=INFO,2=WARN,3=ERROR |
| `name` | 字符串 | DataLogger | 设备名称 |

## 7. 测试流程

### 7.1 基础功能测试

```
1. 上电 → 串口输出启动信息
   预期: [SYS] Init complete → 四任务 started → 欢迎横幅 → > 提示符

2. help → 显示 13 条命令
3. task → 显示 6 个任务 (含 IDLE 和 Tmr Svc)
4. heap → 显示剩余堆内存
5. status → 显示 Flash 使用量
```

### 7.2 日志功能测试

```
1. 观察 DataProducer 每秒产生日志
   预期: [I] cnt=0 temp=xx.x tick=xxx

2. cat log → 查看已写入 Flash 的日志记录
   预期: 每条记录含时间戳、等级、消息内容

3. loglvl 3 → 仅 ERROR 级别日志出现
   loglvl 0 → 所有级别恢复显示

4. 断电重启 → cat log
   预期: 历史日志可回读
```

### 7.3 配置持久化测试

```
1. get sample_ms → 预期: 1000
2. set sample_ms 500 → 立即生效，DataProducer 加速
3. 断电重启 → get sample_ms → 预期: 1000 (未 save)
4. set sample_ms 500 → save → Config saved.
5. 断电重启 → get sample_ms → 预期: 500
```

### 7.4 稳定性测试

```
1. set sample_ms 50 → 高速日志产生
2. 观察 Monitor 输出: Dropped 计数应增长 (队列满丢包)
3. 系统不崩溃、不卡死
4. restore 正常间隔: set sample_ms 1000

5. 连续运行30分钟 → Monitor 显示 Heap/MinHeap 数值稳定
6. 快速连续输入50条 CLI 命令 → 无崩溃
7. erase 执行时 Logger 正常等待锁 → 无 Flash 数据损坏
```

### 7.5 断电保护测试

```
1. set sample_ms 500 → save → 确认 Config saved.
2. 断开电源
3. 重新上电 → get sample_ms → 预期: 500
   (从 B 区或 A 区恢复，版本号机制选最新)
```

## 8. Monitor 输出说明

```
=== System Monitor ===
Tick     : 当前系统 Tick (ms)
Heap     : 当前剩余 / 总堆大小 bytes
MinHeap  : 历史最小剩余堆 (检测内存泄漏)
Flash    : 已用 / 总容量 KB
Dropped  : 因队列满丢弃的日志总数
Tasks    : FreeRTOS 任务状态表
=====================
```

## 9. 故障排查

| 现象 | 可能原因 | 解决 |
|------|----------|------|
| 每次启动打印 "Using default config" | Config_Save 前 g_FlashMutex 为 NULL | 确认 IPC 创建在 Config_Init 之前 |
| CLI 无响应 | UART RX 中断丢失或 TX 竞争 | USART1 优先级 ≥6，BSP_UART 有 TX 互斥锁 |
| 编译 RAM 溢出 | 静态缓冲区过大 | 降低 configTOTAL_HEAP_SIZE 或合并数组 |
| 死机/不停打印溢出 | 任务栈过小 | 增大 TASK_STACK_x 或减少局部变量 |

## 10. 编译配置提醒

- Keil Options → C/C++ → Include Paths 需包含: `..\BSP;..\Utils;..\FS;..\Log;..\CLI;..\Config;..\App;..\User`
- Linker 需确保所有 .c 文件均加入编译列表
- `configTOTAL_HEAP_SIZE` 当前 8192，若需更大 cat 文件缓冲可适当增大
- 所有 CubeMX 生成的 USER CODE 区修改均在 `USART1_MspInit 1` 等处完成，重新生成代码时保留
