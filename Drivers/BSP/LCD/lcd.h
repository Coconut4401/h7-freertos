/**
 * @file lcd.h
 * @brief 提供 LCD 控制器初始化、像素访问和基础图形文字绘制接口。
 * @details 这是 lcd 模块的接口文件（Drivers/BSP/LCD/lcd.h）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#ifndef __LCD_H
/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define __LCD_H

#include "stdlib.h"
#include "./SYSTEM/sys/sys.h"

#define LCD_WR_GPIO_PORT                GPIOD
#define LCD_WR_GPIO_PIN                 SYS_GPIO_PIN5
#define LCD_WR_GPIO_CLK_ENABLE()        do{ RCC->AHB4ENR |= 1 << 3; }while(0)

#define LCD_RD_GPIO_PORT                GPIOD
#define LCD_RD_GPIO_PIN                 SYS_GPIO_PIN4
#define LCD_RD_GPIO_CLK_ENABLE()        do{ RCC->AHB4ENR |= 1 << 3; }while(0)

#define LCD_BL_GPIO_PORT                GPIOB
#define LCD_BL_GPIO_PIN                 SYS_GPIO_PIN5
#define LCD_BL_GPIO_CLK_ENABLE()        do{ RCC->AHB4ENR |= 1 << 1; }while(0)

#define LCD_CS_GPIO_PORT                GPIOD
#define LCD_CS_GPIO_PIN                 SYS_GPIO_PIN7
#define LCD_CS_GPIO_CLK_ENABLE()        do{ RCC->AHB4ENR |= 1 << 3; }while(0)

#define LCD_RS_GPIO_PORT                GPIOD
#define LCD_RS_GPIO_PIN                 SYS_GPIO_PIN13
#define LCD_RS_GPIO_CLK_ENABLE()        do{ RCC->AHB4ENR |= 1 << 3; }while(0)

#define LCD_FMC_NEX         1
#define LCD_FMC_AX          18

#define LCD_FMC_BCRX        FMC_Bank1_R->BTCR[(LCD_FMC_NEX - 1) * 2]
#define LCD_FMC_BTRX        FMC_Bank1_R->BTCR[(LCD_FMC_NEX - 1) * 2 + 1]
#define LCD_FMC_BWTRX       FMC_Bank1E_R->BWTR[(LCD_FMC_NEX - 1) * 2]

/** @brief 模块数据类型：描述本模块维护的状态、配置或数据快照。 */
typedef struct
{
    uint16_t width;
    uint16_t height;
    uint16_t id;
    uint8_t dir;
    uint16_t wramcmd;
    uint16_t setxcmd;
    uint16_t setycmd;
} _lcd_dev;

extern _lcd_dev lcddev;

extern uint32_t  g_point_color;
extern uint32_t  g_back_color;

#define LCD_BL(x)       sys_gpio_pin_set(LCD_BL_GPIO_PORT, LCD_BL_GPIO_PIN, x)

typedef struct
{
    volatile uint16_t LCD_REG;
    volatile uint16_t LCD_RAM;
} LCD_TypeDef;

#define LCD_BASE        (uint32_t)((0X60000000 + (0X4000000 * (LCD_FMC_NEX - 1))) | (((1 << LCD_FMC_AX) * 2) -2))
#define LCD             ((LCD_TypeDef *) LCD_BASE)

#define L2R_U2D         0
#define L2R_D2U         1
#define R2L_U2D         2
#define R2L_D2U         3

#define U2D_L2R         4
#define U2D_R2L         5
#define D2U_L2R         6
#define D2U_R2L         7

#define DFT_SCAN_DIR    L2R_U2D

#define WHITE           0xFFFF
#define BLACK           0x0000
#define RED             0xF800
#define GREEN           0x07E0
#define BLUE            0x001F
#define MAGENTA         0XF81F
#define YELLOW          0XFFE0
#define CYAN            0X07FF

#define BROWN           0XBC40
#define BRRED           0XFC07
#define GRAY            0X8430
#define DARKBLUE        0X01CF
#define LIGHTBLUE       0X7D7C
#define GRAYBLUE        0X5458
#define LIGHTGREEN      0X841F
#define LGRAY           0XC618
#define LGRAYBLUE       0XA651
#define LBBLUE          0X2B12

#define SSD_HOR_RESOLUTION      800
#define SSD_VER_RESOLUTION      480

#define SSD_HOR_PULSE_WIDTH     1
#define SSD_HOR_BACK_PORCH      46
#define SSD_HOR_FRONT_PORCH     210

#define SSD_VER_PULSE_WIDTH     1
#define SSD_VER_BACK_PORCH      23
#define SSD_VER_FRONT_PORCH     22

#define SSD_HT          (SSD_HOR_RESOLUTION + SSD_HOR_BACK_PORCH + SSD_HOR_FRONT_PORCH)
#define SSD_HPS         (SSD_HOR_BACK_PORCH)
#define SSD_VT          (SSD_VER_RESOLUTION + SSD_VER_BACK_PORCH + SSD_VER_FRONT_PORCH)
#define SSD_VPS         (SSD_VER_BACK_PORCH)

/**
 * @brief lcd_wr_data：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param data 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_wr_data(volatile uint16_t data);
/**
 * @brief lcd_wr_regno：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param regno 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_wr_regno(volatile uint16_t regno);
/**
 * @brief lcd_write_reg：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param regno 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param data 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_write_reg(uint16_t regno, uint16_t data);

/**
 * @brief lcd_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void lcd_init(void);
/**
 * @brief lcd_display_on：开启 LCD 显示输出，使已配置图层重新可见。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void lcd_display_on(void);
/**
 * @brief lcd_display_off：关闭 LCD 显示输出，同时保留后续恢复所需的配置。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void lcd_display_off(void);
/**
 * @brief lcd_scan_dir：扫描或采样当前输入与设备状态，整理本轮可用数据。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param dir 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_scan_dir(uint8_t dir);
/**
 * @brief lcd_display_dir：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param dir 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_display_dir(uint8_t dir);
/**
 * @brief lcd_ssd_backlight_set：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param pwm 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_ssd_backlight_set(uint8_t pwm);

/**
 * @brief lcd_write_ram_prepare：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void lcd_write_ram_prepare(void);
/**
 * @brief lcd_set_cursor：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_set_cursor(uint16_t x, uint16_t y);
/**
 * @brief lcd_read_point：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint32_t lcd_read_point(uint16_t x, uint16_t y);
/**
 * @brief lcd_draw_point：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_draw_point(uint16_t x, uint16_t y, uint32_t color);

/**
 * @brief lcd_clear：清除已有状态或复位目标设备，使其回到约定的初始状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_clear(uint16_t color);
/**
 * @brief lcd_fill_circle：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param r 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_fill_circle(uint16_t x, uint16_t y, uint16_t r, uint16_t color);
/**
 * @brief lcd_draw_circle：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x0 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y0 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param r 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_draw_circle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color);
/**
 * @brief lcd_draw_hline：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param len 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_draw_hline(uint16_t x, uint16_t y, uint16_t len, uint16_t color);
/**
 * @brief lcd_set_window：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param sx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param sy 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param width 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param height 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_set_window(uint16_t sx, uint16_t sy, uint16_t width, uint16_t height);
/**
 * @brief lcd_fill：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param sx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param sy 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param ex 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param ey 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint32_t color);
/**
 * @brief lcd_color_fill：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param sx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param sy 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param ex 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param ey 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_color_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color);
/**
 * @brief lcd_draw_line：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x1 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y1 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param x2 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y2 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);
/**
 * @brief lcd_draw_rectangle：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x1 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y1 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param x2 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y2 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_draw_rectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);

/**
 * @brief lcd_show_char：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param chr 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param size 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param mode 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_show_char(uint16_t x, uint16_t y, char chr, uint8_t size, uint8_t mode, uint16_t color);
/**
 * @brief lcd_show_num：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param num 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param len 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param size 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_show_num(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint16_t color);
/**
 * @brief lcd_show_xnum：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param num 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param len 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param size 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param mode 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_show_xnum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t mode, uint16_t color);
/**
 * @brief lcd_show_string：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param width 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param height 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param size 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param p 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_show_string(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, char *p, uint16_t color);

#endif
