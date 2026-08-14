# STM32H743IIT6 mini + 5 英寸电容触摸屏自检程序

## 1. 适用硬件

- 核心板：极客 STM32H743IIT6 mini 版本
- LCD：资料包中的 JUN5030GE-12Q，800 x 480
- 显示接口：STM32 LTDC，RGB565
- 触摸控制器：自动识别 FT/CST（7 位地址 `0x38`）或 GT9xxx（7 位地址 `0x14`）
- 帧缓冲：外部 SDRAM `0xC0000000`

本工程以商家提供的“07 5寸7寸800X480触摸屏实验”为底层驱动基础，针对上述 5 英寸屏固定了正确的 LCD 时序。建议使用原配底板和 LCD 转接板连接。若直接连接裸屏，必须另行满足屏幕背光电源和时序要求，不能把背光 LED 直接接到 MCU GPIO。

## 2. 主要引脚

| 功能 | STM32 引脚 |
| --- | --- |
| LCD DE | PF10 |
| LCD VSYNC | PI9 |
| LCD HSYNC | PI10 |
| LCD PCLK | PG7 |
| LCD 背光使能 | PB5 |
| LCD R3-R7 | PH9、PH10、PH11、PH12、PG6 |
| LCD G2-G7 | PH13、PH14、PH15、PI0、PI1、PI2 |
| LCD B3-B7 | PG11、PI4、PI5、PI6、PI7 |
| 电容触摸 SCL | PH6 |
| 电容触摸 SDA | PI3 |
| 电容触摸 INT | PH7 |
| 电容触摸 RST | PI8 |
| USART1 TX | PA9 |
| USART1 RX | PA10 |
| 红色 LED | PB1，低电平点亮 |
| 绿色 LED | PB0，低电平点亮 |

LCD 和触摸控制器的逻辑电平为 3.3 V，所有设备必须共地。

## 3. LCD 参数

- 分辨率：800 x 480
- 像素格式：RGB565
- 像素时钟：约 33.3 MHz
- HSW / HBP / HFP：1 / 46 / 210
- VSW / VBP / VFP：1 / 23 / 22
- 帧缓冲大小：2,048,000 字节，链接到 SDRAM `0xC0000000`，属性为 `UNINIT`

SDRAM MPU 区域被配置为共享、不可缓存、不可缓冲，避免 Cortex-M7 D-Cache、CPU、DMA2D 和 LTDC 之间出现帧缓冲不一致。

## 4. 直接烧录已生成固件

已生成固件位于：

```text
Output/atk_h743.hex
```

可使用 Keil、STM32CubeProgrammer 或 ST-Link Utility 烧录。使用 STM32CubeProgrammer 时：

1. 通过 SWD 连接 ST-Link 与核心板并给开发板供电。
2. 选择 STM32H743II，连接目标板。
3. 打开 `Output/atk_h743.hex`。
4. 执行 Download，完成后复位开发板。

HEX 已包含 Flash 地址，不需要手工填写偏移。若烧录 AXF，入口地址为 `0x08000299`。

## 5. 使用 Keil 重新编译

1. 安装 Keil MDK 5.43a 或兼容版本。
2. 安装 `Keil.STM32H7xx_DFP.3.0.0` Device Pack。
3. 确认已安装 Arm Compiler 6.24。
4. 打开 `Projects/MDK-ARM/atk_h743.uvprojx`。
5. 选择目标 `TOUCH`，执行 Rebuild。
6. 编译结果位于 `Output`。

本交付版本已经使用 Arm Compiler 6.24 全量编译，结果为 `0 Error(s)`。基础库仍有 49 条静态未使用声明、未使用参数和字符下标类警告，不影响链接或 HEX 生成；详细记录见 `Output/build.log`。

## 6. 上电后的预期现象

程序依次执行：

1. 将主频配置为 400 MHz。
2. 对 SDRAM 的 `0xC0200000` 区域执行两组 16 KiB 数据模式读写测试。
3. 初始化 800 x 480 LCD，显示标题、状态栏、8 色条和触摸网格。
4. 通过 I2C ACK 自动探测 FT/CST 或 GT9xxx；GT9xxx 使用手册规定的复位地址选择时序。
5. 显示识别到的触摸控制器系列或 GT9xxx Product ID。
6. 在网格内触摸时绘制彩色十字和圆点，最多显示 5 个触点。
7. 点击右下角红色 `CLEAR` 区域可重置测试画面。
8. 绿色 LED 约每 0.5 秒翻转一次，表示主循环仍在运行。

若三种候选地址均未应答，LCD 测试仍会继续，状态栏显示 `CTP: NO ACK @38/14/5D`。

40P 直连屏不需要额外转接板或独立触摸排线。程序依次探测 FT/CST 的 7 位地址 `0x38`，以及 GT9xxx 的 `0x14`、`0x5D`；GT9xxx 的两个地址通过复位期间驱动 INT 电平分别选择。

初始化失败时还会释放 SCL/SDA 并扫描全部 `0x08` 到 `0x77` 地址。LCD 显示 `I2C BUS: SCL=x SDA=x` 和 `SCAN DEV=xx FIRST=0xYY`，用于区分总线被拉低、总线空闲但无设备、以及控制器使用其他地址。

诊断地址现在使用真正的十六进制字符显示，并要求同一地址连续三次 ACK 才计入扫描结果。GT9xxx 初始化不再限定旧驱动列出的四个 Product ID；只要地址应答且 `0x8140` 返回至少三个有效 ASCII 字符，就按兼容 Goodix 寄存器协议运行。原始 PID 字节会输出到 USART1。

## 7. 串口输出

串口参数：USART1，115200 bit/s，8 数据位，1 停止位，无校验。

正常输出示例：

```text
STM32H743 5-inch LCD/touch self-test
System clock: 400 MHz
SDRAM test: PASS
LCD timing: 800x480, PCLK 33.3 MHz, RGB565
CTP ID:911
GT911 product ID: 911
```

## 8. 故障指示

- 红色 LED 循环闪 2 次：SDRAM 读写自检失败，程序停止。
- 红色 LED 循环闪 3 次：LCD 初始化后的尺寸不是 800 x 480，程序停止。
- LCD 正常但显示 CTP FAIL：检查 PH6、PI3、PH7、PI8、3.3 V 和 GND，尤其检查触摸排线方向。
- 屏幕全白但背光亮：优先检查 SDRAM、LTDC 数据线和 DE/PCLK；同时通过串口确认是否进入故障灯码。

## 9. 已完成的验证

- Keil Arm Compiler 6.24 全量编译和链接：通过，0 Error。
- AXF、HEX 和 MAP 生成：通过。
- 链接入口：`0x08000299`。
- 帧缓冲地址：`0xC0000000`。
- 帧缓冲大小：`0x001F4000`，链接属性 `UNINIT`。
- 5 英寸 LCD 时序：与 `5寸PT.pdf` 推荐值一致。
- GT911 地址选择复位时序：与 GT911 数据手册第 10 页的 `0x28/0x29` 时序一致。

由于当前环境没有连接实际核心板、LCD 和 ST-Link，无法声称已经完成实物烧录和触摸坐标方向实测。工程已经消除了可通过源码、手册、编译器和链接映射发现的问题；最终硬件确认应以上电显示、串口日志和触摸轨迹为准。
