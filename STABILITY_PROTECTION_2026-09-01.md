# STM32H743 调试与稳定性保护说明

日期：2026-09-01

## 1. 当前验证边界

- CH9350 鼠标拔出检测和重新接入已经由用户完成实物验证。
- 本次改动没有修改 CH9350 协议、鼠标坐标算法、DRAW 文件格式、音频 DMA 缓冲参数、RTC、FILES 或 SETTINGS 数据格式。
- 栈、堆、异常和看门狗保护已经通过 Arm Compiler 6.24 全量编译。
- 诊断模式 1-4 均已分别通过全量编译，但仍需按本文步骤进行实物故障注入。
- 正式固件必须保持 `App/app_diagnostics.h` 中 `APP_DIAGNOSTIC_MODE` 为 `0U`。

## 2. 已实现保护

### 2.1 FreeRTOS

- `configCHECK_FOR_STACK_OVERFLOW = 2`，同时检查栈指针和栈填充值。
- `configUSE_MALLOC_FAILED_HOOK = 1`。
- `configASSERT`、栈溢出和 malloc 失败统一写入故障记录并软件复位。
- assert 记录的任务字段保存源文件名，PC 字段保存源代码行号。
- Input、Gui、Storage、Audio、Monitor、Health 和 Timer 任务均显示历史最小剩余栈。
- 剩余栈低于 128 words 时 MONITOR 标红，并向 LOGS 写入一次预警。
- 显示当前堆和历史最低堆；历史最低堆低于 32 KiB 时写入预警。

### 2.2 Cortex-M7 异常

- 显式启用 BusFault、UsageFault 和除零陷阱。保持 Cortex-M7 默认的非对齐访问兼容行为，避免厂商 LCD/BSP 代码在首屏初始化阶段因严格对齐陷阱反复复位。
- HardFault、BusFault、UsageFault 和 MemManage 保存 CFSR、HFSR、DFSR、AFSR、BFAR、MMFAR、PC、LR 和 SP。
- 支持基本栈帧和浮点扩展栈帧。

### 2.3 故障留存

- 故障记录保存在 `0x38800000` 的 4 KiB Backup SRAM 中，不占用现有 AXI SRAM 链接区。
- 记录包含 magic、版本、长度、序号、故障类型、任务名、Tick、复位标志、异常寄存器和校验和。
- 启动后将故障摘要写入 LOGS。
- StorageTask 使用独立响应队列事务式生成 `CRASH.LOG`；写入成功后才清除 Backup SRAM 记录。
- SD 不可用时每 5 秒重试，故障记录不会因一次写卡失败丢失。

### 2.4 任务心跳和 IWDG

- HealthTask 优先级为 6，每 250 ms 检查五个业务任务。
- Input、Gui、Audio 最大无心跳时间为 1.5/1.5/5 秒；Monitor 为 2 秒；Storage 为 10 秒。
- 所有任务首次健康后才启动 IWDG，硬件超时约 15 秒。
- 只有全部任务健康时喂狗；任务超时时先保存任务名，再停止喂狗。
- 调试器连接时冻结 IWDG，避免断点调试造成无意义复位。
- LSI 和 IWDG 状态等待均限制为 100 ms，等待期间主动让出 CPU；初始化失败时每 5 秒重试，禁止 HealthTask 因硬件状态异常永久占用最高优先级。

### 2.5 队列和存储恢复

- 输入 MOVE 在队列只剩 4 个空位时限流，为 UP、BACK、连接/断连等关键事件保留空间。
- 非 MOVE 关键事件允许最多等待 20 ms，让 GuiTask 获得运行机会并释放队列。
- 分别统计总丢弃、MOVE 限流和关键事件丢弃；关键丢弃必须保持为 0。
- StorageTask 不再永久阻塞在响应队列，响应最多等待 100 ms，超时后计数并继续服务。
- StorageTask 请求队列容量为 8，吸收短时间并发存储请求。
- 统计 Storage 请求队列当前值、峰值、队列满、响应丢弃和文件系统错误。
- `FR_DISK_ERR`、`FR_INT_ERR`、`FR_NOT_READY`、`FR_NOT_ENABLED`、`FR_NO_FILESYSTEM` 和 `FR_TIMEOUT` 会将 Storage 标记为 ERROR；下一请求会重新挂载。

## 3. MONITOR 字段

- `HEAP NOW / MIN`：当前堆和启动以来历史最低堆，单位 bytes。
- `DROP TOTAL / SEC`：MOVE 限流等累计丢弃数/最近统计窗口折算的每秒丢弃数；下方 `CRIT` 为关键事件丢弃数。
- `INPUT QUEUE NOW / PEAK`：输入队列当前深度和历史峰值。
- `STACK WORDS I/G/S/A/M + H/T`：Input、Gui、Storage、Audio、Monitor、Health、Timer 的历史最小剩余栈，单位 words。
- `HEALTH`：健康任务掩码/超时任务掩码；正常为 `31/0`。
- `WDG`：IWDG 是否已经进入受控喂狗状态。
- `RST`：本次启动前记录的故障类型，正常冷启动为 0。
- `SD Q/P`：Storage 请求队列当前值/峰值。
- `QFULL`、`RESPERR`、`FSERR`：Storage 队列满、响应丢弃、文件系统错误计数。

## 4. 诊断构建

修改 `App/app_diagnostics.h` 中的 `APP_DIAGNOSTIC_MODE`，每次只测试一种模式。

| 模式 | 故障注入 | 预期结果 |
| ---: | --- | --- |
| 0 | 正式固件 | 不创建 DiagTask |
| 1 | DiagTask 递归消耗栈 | 触发栈溢出钩子并复位，故障类型 2 |
| 2 | 连续申请 4096 bytes | 触发 malloc 失败钩子并复位，故障类型 3 |
| 3 | 挂起 InputTask | HealthTask 记录 InputTask，IWDG 随后复位，故障类型 8 |
| 4 | 填满输入队列 | 系统继续运行，队列能够排空，关键事件恢复投递 |

模式 1-3 触发复位后应立即烧录模式 0 固件，避免诊断任务每次启动后再次触发。随后检查：

1. LOGS 中出现 `RESET TYPE ...` 和 `CRASH.LOG SAVED TO SD CARD`。
2. SD 根目录 `CRASH.LOG` 包含故障类型、任务名、复位标志和异常寄存器。
3. MONITOR 的 `RST` 与注入模式相符。
4. 故障后系统能够进入登录和桌面，不通过永久死循环模拟保护。

## 5. 实物稳定性验收记录

### 5.1 高频输入 60 秒

操作：连续快速移动鼠标、滚轮滚动并点击应用图标 60 秒。

通过条件：

- 光标无延迟追赶、无粘住的按下状态、无复位。
- `CRITICAL = 0`。
- 输入队列峰值不超过 32，停止操作后 `NOW` 回到 0。
- MOVE 限流允许增加，但必须记录最终数值。

记录：固件 SHA-256、TOTAL、CRITICAL、QUEUE NOW/PEAK、七个栈余量。

### 5.2 前后台并发

操作：播放问题 WAV，同时保存 DRAW、导出 LOGS、刷新 FILES，并持续移动和点击鼠标。

通过条件：

- 鼠标和触摸持续响应，WAV 无新增长时间颤音。
- `HEALTH = 31/0`、`WDG = ON`、`RESPERR = 0`。
- 所有栈余量大于等于 128 words，历史最低堆大于等于 32768 bytes。

### 5.3 资源上限

依次将 DRAW 点数、LOGS 条数、文件数和输入事件压力推到上限。

通过条件：应用显示明确的满载提示；系统不越界、不复位；清空或删除后可继续操作。

### 5.4 存储异常

操作：执行一次保存时拔出 SD 卡，等待失败提示，再插回 SD 卡并重试。

通过条件：

- 本次操作明确失败并增加 `FSERR`，StorageTask 和输入界面不死锁。
- 插卡后的下一次请求重新挂载并成功。
- 原目标文件为旧完整版本或新完整版本，不接受半写文件。
- `QFULL` 和 `RESPERR` 若增加，必须记录原因；恢复后不再持续增加。

### 5.5 长时间运行 60 分钟

每 10 分钟记录一次：运行时间、堆 NOW/MIN、七个任务栈、输入队列、Storage 队列、所有错误计数和鼠标状态。

通过条件：

- 无意外复位，`RST` 不变化，`HEALTH = 31/0`。
- 历史最低堆只下降到稳定平台，不持续下降。
- 栈余量均不低于 128 words。
- `CRITICAL = 0`、`RESPERR = 0`，错误计数无无法解释的持续增长。

### 5.6 回归

稳定性固件必须重新验证：鼠标拔出/接入、DRAW 快速画线、原问题 WAV 播放 5-10 分钟、RTC 手动校时、SETTINGS 重启保持、FILES/DRAW/LOGS 事务保存。

## 6. 结果填写

| 测试 | 日期 | 固件 SHA-256 | 结果 | 关键数据/现象 |
| --- | --- | --- | --- | --- |
| 高频输入 60 秒 |  |  | 未测试 |  |
| 前后台并发 |  |  | 未测试 |  |
| 资源上限 |  |  | 未测试 |  |
| 存储异常 |  |  | 未测试 |  |
| 栈溢出注入 |  |  | 未测试 |  |
| 堆耗尽注入 |  |  | 未测试 |  |
| 任务停滞/IWDG |  |  | 未测试 |  |
| 输入队列注入 |  |  | 未测试 |  |
| 连续运行 60 分钟 |  |  | 未测试 |  |
| 完整功能回归 |  |  | 未测试 |  |

## 7. 正式构建记录

```text
Target: TOUCH
Rebuild: 2026-09-01 16:55:32
Arm Compiler: 6.24
Program Size: Code=139936 RO-data=22264 RW-data=124 ZI-data=2418964
Result: 0 Error(s), 67 Warning(s)
HEX size: 456738 bytes
HEX SHA-256: EC9C88C8A921F27B04184A8AE66FCA3107307FA6FE7565CF410435F3E8760A40
```

67 条警告与修改前基线数量相同，仍主要来自厂商 BSP 头文件、未使用参数和旧代码风格。诊断模式 1、2、3、4 也分别完成过全量编译并达到 `0 Error(s)`；上表正式 HEX 为模式 0。

2026-09-01 上板首次发现启动阶段白屏反复闪烁。排查确认新增的 `UNALIGN_TRP` 在 `lcd_init()` 之前启用，与工程现有厂商 LCD/BSP 的非对齐访问兼容要求存在风险。正式版本已撤销该严格陷阱，只保留除零、BusFault、UsageFault、MemManage、FreeRTOS 栈/堆钩子、故障留存和 IWDG。修正后重新全量编译，结果和哈希如上；仍需重新烧录进行实物确认。

同次上板继续发现启动首页不能进入密码页。启动状态机与修改前一致，定位到最高优先级 HealthTask 的 LSI/IWDG 初始化使用无超时忙等待，存在永久阻塞低优先级 GuiTask 的路径。现已改为有界、可让出 CPU、失败后延时重试的初始化流程；正式构建记录应以最后一次全量 Rebuild 为准。
