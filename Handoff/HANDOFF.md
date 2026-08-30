# STM32H743 微型桌面运行时项目交接说明

## 1. 当前基线

| 项目 | 当前值 |
| --- | --- |
| MCU | STM32H743IIT6，Cortex-M7 |
| 核心板 | 正点原子/ATK STM32H743IIT6 mini 类核心板 |
| 主频 | 400 MHz |
| 工程目录 | `E:\STM32Project\STM32H743_5inch_GT911_Test` |
| Keil 工程 | `Projects\MDK-ARM\atk_h743.uvprojx` |
| Target | `TOUCH` |
| IDE | Keil uVision 5.43.1 |
| 编译器 | Arm Compiler 6.24 / ARMCLANG |
| Keil 路径 | `D:\stm app\UV4\UV4.exe` |
| 固件 | `Output\atk_h743.hex` |
| 版本管理 | 当前不是 Git 仓库 |

交接前的最新源码包含 CH9350 鼠标和 DS3231 校时修复。交接构建结果：

```text
Program Size: Code=131782 RO-data=21298 RW-data=104 ZI-data=2270728
"..\..\Output\atk_h743.axf" - 0 Error(s), 66 Warning(s).
Build Time Elapsed: 00:00:05
```

66 条警告来自原有 BSP 的未使用静态函数、参数和初始化提示，没有本次交接构建错误。最终 SHA-256 以包内 `manifest/SHA256SUMS.txt` 为准。

## 2. 硬件配置与接线

### 2.1 供电和下载

- 核心板从板载 `5V/VIN` 输入 5 V，MCU GPIO 电平为 3.3 V，不能给 GPIO 输入 5 V。
- 所有外设与核心板必须共地。
- 使用外部 5 V 供电时，不要再让 ST-Link 电源脚同时给核心板供电。
- ST-Link 最少连接 `SWDIO/TMS`、`SWCLK/TCK`、`GND/VSS`。烧录时可接 `NRST`。
- 此前实物出现过 ST-Link 的 NRST 持续拉低目标板的情况；脱机运行异常时可拔掉 NRST 排查。

### 2.2 LCD 和触摸

5 英寸屏通过 40P 排线直连核心板，当前不使用外接转接板，也不需要另接触摸排线。

| 参数 | 当前配置 |
| --- | --- |
| LCD | JUN5030GE-12Q，代码强制使用 LCD ID `0x7084` 时序 |
| 分辨率 | 800 x 480 横屏 |
| 接口 | LTDC RGB565 |
| 像素时钟 | 约 33.3 MHz |
| 时序 | HSW/HBP/HFP = 1/46/210；VSW/VBP/VFP = 1/23/22 |
| 帧缓冲 | 外部 SDRAM `0xC0000000`，驱动保留 1280 x 800 RGB565 缓冲区 |
| 背光 | PB5，TIM3_CH2 PWM，设置范围 1% 到 100% |

主要 LTDC 引脚：

| 信号 | MCU 引脚 |
| --- | --- |
| DE | PF10 |
| VSYNC | PI9 |
| HSYNC | PI10 |
| PCLK | PG7 |
| R3-R7 | PH9、PH10、PH11、PH12、PG6 |
| G2-G7 | PH13、PH14、PH15、PI0、PI1、PI2 |
| B3-B7 | PG11、PI4、PI5、PI6、PI7 |

触摸实物已识别为 Goodix `GT927`，7 位 I2C 地址 `0x14`：

| 信号 | MCU 引脚 |
| --- | --- |
| SCL | PH6 |
| SDA | PI3 |
| INT | PH7 |
| RST | PI8 |

驱动还会兼容探测 FT/CST `0x38` 和 GT9xxx `0x5D`，但当前实物是 `GT927/0x14`。

### 2.3 microSD

使用核心板板载 SD1 卡槽，卡格式为 FAT32。

| SDMMC1 信号 | MCU 引脚 |
| --- | --- |
| D0 | PC8 |
| D1 | PC9 |
| D2 | PC10 |
| D3 | PC11 |
| CLK | PC12 |
| CMD | PD2 |

已验证约 15,200 MB 卡容量、FAT32 挂载、`TEST.TXT` 读取和 `STM32_OK.TXT` 写回。底层 SDMMC 当前采用轮询，传输期间会关闭总中断，这是音频和实时响应的已知风险。

### 2.4 DS3231

| DS3231 | 连接 |
| --- | --- |
| VCC | 3.3 V |
| GND | GND |
| SCL | PH4 |
| SDA | PH5 |
| 后备电池 | CR2032 |

- DS3231 7 位地址为 `0x68`。
- PH4/PH5 是软件 I2C，总线上启动时还探测 AT24C02 `0x50`；当前 `AT24C02: NO ACK` 不影响 DS3231。
- `APP_RTC_COMPENSATION_PPM` 当前为 `0`。
- 旧逻辑会在每个新固件首次启动时用编译时间覆盖 RTC，因此编译到烧录的间隔会表现为固定慢若干分钟。
- 新逻辑只在 RTC 数据无效或状态寄存器 `OSF` 置位时使用编译时间初始化；正常烧录不再覆盖有效时间。
- SETTINGS 中新增 `TIME` 页面，可调年月日时分秒；`APPLY` 会自动计算星期、写 DS3231、清除 OSF，并立即更新软件缓存。
- 当前 RTC 修复已编译通过，尚待实物验证。由于旧 RTC 中已经是慢的时间，烧录新固件后需要进入 `SETTINGS -> TIME` 手动校准一次。

### 2.5 MAX98357A 和扬声器

| MAX98357A | STM32H743/电源 |
| --- | --- |
| VIN/VCC | 5 V |
| GND | 公共 GND |
| LRC/LRCLK/WS | PB12，SPI2/I2S2_WS |
| BCLK | PB13，SPI2/I2S2_CK |
| DIN | PB15，SPI2/I2S2_SDO |
| GAIN | 悬空，模块默认增益 |
| SD/EN | 悬空，模块默认使能 |

扬声器为 4 欧 / 3 W，两个端子分别接 `SPK+`、`SPK-`，任何一端都不能接 GND。当前音频 DMA 缓冲为 16384 个 `int16_t`，带 D-Cache clean；440 Hz 测试音已实物验证。

### 2.6 CH9350 USB 鼠标

CH9350 模块使用 5 V 供电，UART 与 STM32H743 的 USART1 相连：

| CH9350 模块 | STM32H743 |
| --- | --- |
| 5V | 5 V |
| GND | GND |
| TX | PA10 / USART1_RX |
| RX | PA9 / USART1_TX |

说明：

- 最小鼠标接收只依赖模块 TX -> PA10、5V 和 GND；模块 RX 用于接收主机命令。
- PA9 同时是工程的 `printf` 调试输出。CH9350 接收脚接在 PA9 时会收到这些日志；如需独立串口观察日志，应使用高阻监听或另行规划调试串口。
- 不要让 CH9350 TX 和另一个 USB-TTL 的 TX 同时推挽驱动 PA10。
- UART 配置为 `115200 8N1`，USART1 RX 使用 256 字节中断环形缓冲区，中断优先级 6。

当前工作模式的逻辑电平：

```text
下位机模式
S0 = 低
S1 = 高
BAUD0 = 高
BAUD1 = 高
UART = 115200 8N1
工作状态 = 状态 2
```

物理拨码的 `ON` 与高/低电平关系必须以模块丝印和资料为准，不要只按开关方向猜测。用户当前已经找到可用配置，非必要不要改变实物拨码。

当前解析的相对鼠标帧为 7 字节：

```text
57 AB 02 Button DeltaX DeltaY Wheel
```

`DeltaX/DeltaY/Wheel` 按有符号 8 位数处理。D2 LED 在移动时闪烁只说明 USB 侧有活动，并不能单独证明 UART 协议已经被 MCU 正确解析。

## 3. 软件结构

### 3.1 FreeRTOS

- 抢占式调度，1 kHz Tick。
- `heap_4.c`，总堆 128 KB。
- 最大优先级数 8。
- 输入事件队列长度 32。
- Cortex-M7 I-Cache/D-Cache 已开启。
- LCD SDRAM 帧缓冲通过 MPU 配置为不可缓存，避免 CPU、DMA2D 和 LTDC 数据不一致。

| 任务 | 栈深度 | 优先级 | 作用 |
| --- | ---: | ---: | --- |
| InputTask | 512 | 4 | 每 5 ms 扫描触摸并读取 CH9350，生成统一输入事件 |
| StorageTask | 1024 | 4 | FatFs 唯一所有者，处理文件、配置、日志、绘图和音频读取 |
| GuiTask | 1024 | 3 | 登录、桌面、应用路由、UI 和 RTC 更新 |
| AudioTask | 1024 | 3 | WAV 控制、I2S DMA 双半缓冲填充 |
| MonitorTask | 512 | 1 | 500 ms 状态采样、LED 和可选串口状态输出 |

不要从其他任务直接调用 FatFs；现有设计通过 StorageTask 请求/响应完成文件系统操作。

### 3.2 启动流程

1. 配置 400 MHz 系统时钟、USART1、LED 和 MPU。
2. 初始化 SDRAM 并执行两组读写自检。
3. 初始化 LCD，检查 800 x 480 几何尺寸。
4. 探测 FT/CST 或 GT9xxx 触摸控制器；当前应得到 GT927/`0x14`。
5. 在 PH4/PH5 探测 AT24C02 `0x50`。
6. 创建 Storage、Audio、Input、Gui、Monitor 任务并启动 FreeRTOS。
7. StorageTask 挂载 SD 并执行读写自检。
8. InputTask 初始化触摸、CH9350 和鼠标状态。
9. GuiTask 初始化屏幕、DS3231、设置和桌面运行时。

### 3.3 应用模块

| 模块 | 文件 | 当前功能 |
| --- | --- | --- |
| 输入 | `App/app_input.*` | 触摸与鼠标统一事件，DOWN/MOVE/UP/SCROLL/BACK，输入统计 |
| CH9350 | `Drivers/BSP/CH9350/ch9350.*` | UART 帧同步、状态 2 相对鼠标帧解析、协议统计 |
| 鼠标 | `App/app_mouse.*` | 相对位移、边界限制、灵敏度、按键、拖动、滚轮事件 |
| 运行时 | `App/app_runtime.*` | 启动、登录、锁定、桌面、应用切换、熄屏和唤醒 |
| UI | `App/app_ui.*` | 页面绘制、光标和控件坐标命中 |
| 文件 | `App/app_files.*` | 列表、分页、新建、读取、编辑、保存、删除、重名检查 |
| 绘图 | `App/app_draw.*` | 画线、颜色、清空、保存和打开 |
| 音乐 | `App/app_audio.*`、`app_music.*` | SD WAV 扫描、播放/暂停/停止/切歌、音量、测试音 |
| 日志 | `App/app_logs.*` | 64 条运行日志、分页、两步清空、导出 SD |
| 监控 | `App/app_monitor.*` | Tick、空闲堆、队列、输入丢包和任务栈余量 |
| 设置 | `App/app_settings.*` | 灵敏度、光标大小、亮度、音量、熄屏、串口开关、RTC 时间 |
| RTC | `App/app_rtc.*` | DS3231 读取、有效性检查、手动写入、OSF 处理、ppm 框架 |
| 存储 | `App/app_storage.*` | SDMMC/FatFs 串行服务和事务式文件替换 |

### 3.4 操作方式

- 启动画面约 2 秒后进入登录。
- 登录 PIN：`1234`。
- 连续错误 3 次锁定 10 秒。
- 桌面应用：FILES、DRAW、MUSIC、LOGS、MONITOR、SETTINGS。
- 左键用于选中、打开、按钮操作和拖动；右键按下产生返回事件。
- 滚轮在 FILES 列表和 LOGS 页面翻页。
- 触摸与鼠标可同时使用，触摸位置会同步鼠标内部位置，避免光标跳回旧坐标。
- 熄屏只关闭背光，不重启系统；输入可唤醒并保留页面状态。

### 3.5 SD 卡文件

| 文件 | 用途 |
| --- | --- |
| `SETTINGS.CFG` | 设置持久化，格式版本 3；RTC 时间不存入该文件 |
| `DRAWING.DRW` | 绘图数据，格式版本 1，最多 1024 点，带 CRC32 |
| `SYSTEM.LOG` | LOGS 页面手动导出的文本日志 |
| `TEST.TXT` | 上电 SD 读取自检输入，可选 |
| `STM32_OK.TXT` | 上电 SD 写入自检输出 |
| 根目录 `*.WAV` | MUSIC 扫描的音频，最多列出 8 首 |

WAV 支持 RIFF/WAVE PCM、16 位、单声道或双声道，采样率 16/32/44.1/48 kHz。建议使用 8.3 文件名，如 `TEST.WAV`。

## 4. 最近修改

### 4.1 CH9350 鼠标

新增和修改的核心文件：

- `Drivers/BSP/CH9350/ch9350.c/.h`
- `Drivers/SYSTEM/usart/usart.c/.h`
- `Drivers/SYSTEM/usart/usart_rx.h`
- `App/app_mouse.c/.h`
- `App/app_input.c/.h`
- `App/app_runtime.c`
- `App/app_files.c`
- `App/app_logs.c`
- `Projects/MDK-ARM/atk_h743.uvprojx`

最初鼠标 USB 活动时 D2 会闪，但屏幕光标不动。根因是固件解析的帧格式与模块实际状态 2 输出不一致。当前按 `57 AB 02` 7 字节帧解析后，用户已经确认鼠标可以工作。

当前支持：

- 相对移动和屏幕边界限制。
- 左键按下/释放和拖动。
- 右键返回。
- FILES/LOGS 滚轮翻页。
- LOW/NORMAL/HIGH 三档灵敏度。
- 5 ms 周期内合并多个移动报告，按键和滚轮前先刷新待发送移动。
- 触摸和鼠标共存。

当前没有实现键盘帧和绝对鼠标帧的业务处理；解析器知道其长度，但会丢弃。CH9350 状态帧也尚未转成连接/断开事件。

### 4.2 DS3231 时间修复

修改文件：

- `App/app_rtc.c/.h`
- `App/app_settings.c`
- `App/app_ui.c/.h`

完成内容：

- 删除 RTC RAM 固件签名和“新固件必定重设时间”的逻辑。
- 仅在时间无效或 OSF 置位时按编译时间初始化。
- 增加 `app_rtc_set_datetime()`，写入前校验日期并自动计算星期。
- SETTINGS 增加 TIME 页面，支持年月日时分秒加减、CANCEL 和 APPLY。
- 年月变化会自动限制日期，支持 2000-2099 年闰年。
- APPLY 后清 OSF 并更新当前缓存。
- 保持 `SETTINGS.CFG` 版本和格式不变，避免产生第二时间源。

该修改已经通过 Keil 全量编译，但尚未收到上板复测结果。

## 5. 验证状态

### 5.1 实物已确认

- 400 MHz 启动和 SDRAM 自检。
- 800 x 480 LCD 显示。
- GT927 触摸、触摸光标和触摸操作。
- FreeRTOS 多任务运行和 MonitorTask 输出。
- microSD FAT32 挂载、读取和写回。
- FILES、DRAW、LOGS、MONITOR、SETTINGS 的阶段性实测。
- 设置立即生效并可通过 SD 卡持久化。
- 主动/超时熄屏、输入唤醒和页面状态保持。
- DS3231 原有读取与显示。
- MAX98357A 440 Hz 测试音。
- 合规 WAV 可以播放。
- CH9350 鼠标移动、点击等基本输入已经由用户确认可用。

### 5.2 编译通过但待实物复测

- RTC 有效时间不再被新固件覆盖。
- SETTINGS -> TIME 手动校时和断电保持。
- 烧录新固件后 DS3231 时间继续保持。
- MUSIC 页面局部刷新和较大 DMA 缓冲修改后的长时间连续播放稳定性。

### 5.3 考核基础要求的明确遗漏或不足

1. CH9350 连接/断开检测没有接入桌面状态，也没有记录断开和恢复日志。`0x80` 状态帧目前只被识别长度后丢弃。
2. CH9350 解析统计 `ch9350_get_stats()` 尚未接入 MONITOR 页面；当前监控主要显示统一输入队列统计。
3. 尚未形成考核要求的完整测试记录：60 秒高频输入、前后台并发、资源上限、60 分钟运行、异常注入。
4. 工程尚未使用 Git，和考核“使用 Git 进行版本管理”的要求不一致。
5. 开发过程中的 bug 和排查记录分散在对话与交接文档中，尚未整理为正式的 Markdown 开发日志。

### 5.4 进阶项状态

- 已有部分输入事件合并、队列满丢弃计数和存储临时/备份文件替换。
- 未实现任务心跳/看门狗恢复、真实低功耗、OTA/伪 OTA。
- 尚未完成系统化运行负载分析。

## 6. 已知风险和注意事项

- 当前不是 Git 仓库，修改前需保留快照，不要覆盖用户现有改动。
- SDMMC 驱动在轮询传输期间关闭总中断，可能影响 I2S DMA 和输入实时性。
- LCD 为单帧缓冲；局部刷新降低了闪烁，但页面切换不具备双缓冲/VSync 原子交换。
- USART1 同时承担 CH9350 和 `printf`，调试串口与模块接线需避免 TX 冲突。
- DS3231 完全断电后若 OSF 再次置位，应检查 CR2032 电池、电池座和模块是否存在充电电路。
- CH9350 D2 灯反映 USB 活动，不是 MCU 解码成功指示。
- 用户确认鼠标可用，但没有记录当前拨码的机械 ON/OFF 位置；后续以逻辑电平和现场可用配置为准。

## 7. 构建与烧录

### 7.1 Keil GUI

1. 打开 `E:\STM32Project\STM32H743_5inch_GT911_Test\Projects\MDK-ARM\atk_h743.uvprojx`。
2. 选择 Target `TOUCH`。
3. 执行 Rebuild。
4. 确认 `0 Error(s)`，并检查 `Output\atk_h743.hex` 时间戳更新。
5. 使用 ST-Link 烧录 HEX/AXF。

### 7.2 命令行全量构建

```powershell
& 'D:\stm app\UV4\UV4.exe' -j0 -r `
  'E:\STM32Project\STM32H743_5inch_GT911_Test\Projects\MDK-ARM\atk_h743.uvprojx' `
  -t 'TOUCH' `
  -o 'E:\STM32Project\STM32H743_5inch_GT911_Test\Output\handoff_rebuild.log'
```

Keil 命令行启动后可能立即返回，但 UV4 仍在后台构建；必须等待日志出现最终 `Error(s)` 行和 HEX 时间戳更新。

## 8. 下一步建议顺序

1. 烧录交接固件，进入 `SETTINGS -> TIME` 校准一次时间。
2. 验证桌面立即显示新时间、断电重启保持、再次烧录后不覆盖。
3. 保持当前 CH9350 拨码，复测移动、左键、拖动、右键返回、FILES/LOGS 滚轮。
4. 根据考核基础要求实现 CH9350 断开/恢复状态、桌面提示和日志。
5. 把 CH9350 统计加入 MONITOR，方便异常注入和 60 秒输入测试。
6. 按考核清单完成并记录五类稳定性测试。
7. 在用户明确同意后初始化 Git，并整理正式 `DEVELOPMENT_LOG.md`。

后续修改应保持范围最小，不要为了补一个功能重写已经实物可用的触摸、文件、音频或 UI 架构。
