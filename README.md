# STM32H743 FreeRTOS 触摸桌面系统

这是一个运行在 STM32H743IIT6 上的 800 × 480 嵌入式桌面系统。项目从 LCD/电容触摸自检程序逐步发展而来，当前集成了 FreeRTOS、多源输入、SD 卡文件与日志管理、WAV 音频播放、RTC 设置、绘图以及运行状态监控。

> 当前仓库保留了从最初硬件验证到桌面系统的完整 Git 提交历史。旧版内容可通过提交记录和标签 `archive/before-local-import-2026-09-09` 查看。

## 主要功能

- 800 × 480 RGB565 LCD 桌面界面
- GT9xxx/FT/CST 电容触摸控制器探测与输入
- CH9350 USB 鼠标输入，支持移动、点击、拖动、滚轮和右键返回
- FreeRTOS 多任务调度与统一输入事件队列
- SD 卡文件浏览、文本编辑、绘图保存、日志和配置持久化
- WAV 文件播放与 I2S DMA 双半缓冲
- DS3231 RTC 读取和时间设置
- SYSTEM/INPUT 监控页面、输入合并、限流和压力测试
- 栈溢出、堆耗尽、Cortex-M7 异常留存、任务心跳和 IWDG 保护
- 临时文件与备份文件配合的存储事务恢复

## 硬件

| 项目 | 配置 |
| --- | --- |
| MCU | STM32H743IIT6，Cortex-M7，400 MHz |
| 显示屏 | JUN5030GE-12Q，800 × 480 |
| 显示接口 | LTDC，RGB565 |
| 帧缓冲 | 外部 SDRAM，起始地址 `0xC0000000` |
| 触摸 | 实物为 GT927，兼容 GT9xxx/FT/CST 探测 |
| 鼠标 | CH9350，USART1，115200 8N1 |
| 存储 | microSD，FAT32，FatFs |
| RTC | DS3231 |
| 工程 | Keil MDK，Target `TOUCH` |

CH9350 接线：

```text
CH9350 5V  -> STM32 5V
CH9350 GND -> STM32 GND
CH9350 TX  -> PA10 / USART1_RX
CH9350 RX  -> PA9  / USART1_TX
```

USART1 同时用于 CH9350 和调试输出。USB-TTL 仅监听日志时，只连接 PA9、RXD 和 GND；不要将 USB-TTL TXD 与 CH9350 TX 同时连接到 PA10。所有串口信号必须为 3.3 V TTL 电平。

## 软件结构

| 任务 | 栈深度 | 优先级 | 职责 |
| --- | ---: | ---: | --- |
| InputTask | 512 | 4 | 触摸、CH9350 和输入事件生成 |
| StorageTask | 1024 | 4 | FatFs、配置、日志、绘图和音频请求 |
| GuiTask | 1024 | 3 | 登录、桌面、应用路由和界面更新 |
| AudioTask | 1024 | 3 | WAV 控制和 I2S DMA 缓冲 |
| MonitorTask | 512 | 1 | 状态采样、LED 和串口状态输出 |
| HealthTask | 512 | 6 | 任务心跳检查和 IWDG 控制 |

主要目录：

```text
App/                    桌面应用与 FreeRTOS 业务模块
Drivers/                CMSIS、板级和外设驱动
Middlewares/FatFs/      FAT 文件系统
Middlewares/FreeRTOS-Kernel-main/
                        构建所需的 FreeRTOS 内核、头文件和 CM7 端口
Projects/MDK-ARM/       Keil 工程
User/                   main.c 和 FreeRTOSConfig.h
docs/development/       开发、稳定性和测试记录
docs/learning/          FreeRTOS 学习笔记
docs/archive/           历史交接文档
artifacts/              可直接烧录的固件及校验值
```

## 编译

1. 安装 Keil MDK 5.43a 或兼容版本。
2. 安装 `Keil.STM32H7xx_DFP.4.1.3` 或兼容版本 Device Pack。
3. 确认已安装 Arm Compiler 6.24。
4. 打开 `Projects/MDK-ARM/atk_h743.uvprojx`。
5. 选择 `TOUCH` Target，执行 Rebuild。

构建输出位于本地 `Output/`，该目录属于可再生成内容，不纳入后续 Git 提交。

## 烧录

当前固件位于：

```text
artifacts/atk_h743.hex
```

可使用 Keil 或 STM32CubeProgrammer 通过 ST-Link 烧录。HEX 已包含 Flash 地址，无需手动填写偏移。对应 SHA-256 记录在 `artifacts/SHA256SUMS.txt`。

## 当前验证状态

- 最新本地工程已由 Keil Arm Compiler 6.24 生成 AXF 和 HEX，构建记录为 0 Error。
- CAPTURE 60S 实机测试通过：RAW 5188、SENT 4301、MERGED 897、DROP 0、CRIT 0，最大延迟 15 ms。
- FLOOD 10S 的页面和鼠标隔离现象测试通过，完整数值仍待补充。
- 栈/堆保护、故障留存、任务心跳、IWDG 和存储事务机制已经实现，部分故障注入与长时间测试仍待完成。

详细状态、问题复盘和后续验收项目请查看：

- [统一开发日志](docs/development/DEVELOPMENT_LOG.md)
- [输入调度开发日志](docs/development/ADVANCED_INPUT_SCHEDULER_DEVELOPMENT_LOG.md)
- [稳定性保护说明](docs/development/STABILITY_PROTECTION_2026-09-01.md)

## 已知问题

- INPUT 页面测试倒计时在 SYSTEM/INPUT 切换时存在坐标跳动，已定位为首次绘制和动态刷新坐标不一致。
- WAV 播放期间偶发暂停或退出仍在做输入源隔离测试。
- RTC 在再次烧录和断电后的保持行为需要最终实物复测。
- SDMMC 轮询传输期间关闭总中断，可能影响输入和 I2S DMA 的实时性。
- 当前 LCD 使用单帧缓冲，页面切换没有双缓冲/VSync 原子交换。

## 第三方代码

项目包含 ST CMSIS、FreeRTOS Kernel、FatFs 以及原开发板 BSP 代码。FreeRTOS 的许可证文件保留在 `Middlewares/FreeRTOS-Kernel-main/LICENSE.md`。使用或再发布其他第三方组件前，请同时核对其源文件头和原供应方条款。
