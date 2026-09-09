/**
 * @file lcd.c
 * @brief 提供 LCD 控制器初始化、像素访问和基础图形文字绘制接口。
 * @details 这是 lcd 模块的实现文件（Drivers/BSP/LCD/lcd.c）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#include "stdlib.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/LCD/lcdfont.h"
#include "./SYSTEM/usart/usart.h"
#include "./BSP/LCD/ltdc.h"

#include "./BSP/LCD/lcd_ex.c"

uint32_t g_point_color = 0XF800;
uint32_t g_back_color  = 0XFFFF;

_lcd_dev lcddev;

/**
 * @brief lcd_wr_data：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param data 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_wr_data(volatile uint16_t data)
{
    data = data;
    LCD->LCD_RAM = data;
}

/**
 * @brief lcd_wr_regno：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param regno 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_wr_regno(volatile uint16_t regno)
{
    regno = regno;
    LCD->LCD_REG = regno;
}

/**
 * @brief lcd_write_reg：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param regno 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param data 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_write_reg(uint16_t regno, uint16_t data)
{
    LCD->LCD_REG = regno;
    LCD->LCD_RAM = data;
}

/**
 * @brief lcd_rd_data：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint16_t lcd_rd_data(void)
{
    volatile uint16_t ram;
    ram = LCD->LCD_RAM;
    return ram;
}

/**
 * @brief lcd_opt_delay：等待指定时长或硬件条件，以满足总线时序与同步要求。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param i 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void lcd_opt_delay(uint32_t i)
{
    while (i--);
}

/**
 * @brief lcd_write_ram_prepare：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void lcd_write_ram_prepare(void)
{
    LCD->LCD_REG = lcddev.wramcmd;
}

/**
 * @brief lcd_read_point：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint32_t lcd_read_point(uint16_t x, uint16_t y)
{
    uint16_t r = 0, g = 0, b = 0;

    if (x >= lcddev.width || y >= lcddev.height)return 0;

    if (lcdltdc.pwidth != 0)
    {
        return ltdc_read_point(x, y);
    }

    lcd_set_cursor(x, y);

    if (lcddev.id == 0X5510)
    {
        lcd_wr_regno(0X2E00);
    }
    else
    {
        lcd_wr_regno(0X2E);
    }

    r = lcd_rd_data();

    if (lcddev.id == 0X1963)return r;

    lcd_opt_delay(2);
    r = lcd_rd_data();
    if (lcddev.id == 0X7796) return r;

    lcd_opt_delay(2);
    b = lcd_rd_data();
    g = r & 0XFF;
    g <<= 8;
    return (((r >> 11) << 11) | ((g >> 10) << 5) | (b >> 11));
}

/**
 * @brief lcd_display_on：开启 LCD 显示输出，使已配置图层重新可见。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void lcd_display_on(void)
{
    if (lcdltdc.pwidth != 0)
    {
        ltdc_switch(1);
    }
    else if (lcddev.id == 0X5510)
    {
        lcd_wr_regno(0X2900);
    }
    else
    {
        lcd_wr_regno(0X29);
    }
}

/**
 * @brief lcd_display_off：关闭 LCD 显示输出，同时保留后续恢复所需的配置。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void lcd_display_off(void)
{
    if (lcdltdc.pwidth != 0)
    {
        ltdc_switch(0);
    }
    else if (lcddev.id == 0X5510)
    {
        lcd_wr_regno(0X2800);
    }
    else
    {
        lcd_wr_regno(0X28);
    }
}

/**
 * @brief lcd_set_cursor：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_set_cursor(uint16_t x, uint16_t y)
{
    if (lcddev.id == 0X1963)
    {
        if (lcddev.dir == 0)
        {
            x = lcddev.width - 1 - x;
            lcd_wr_regno(lcddev.setxcmd);
            lcd_wr_data(0);
            lcd_wr_data(0);
            lcd_wr_data(x >> 8);
            lcd_wr_data(x & 0XFF);
        }
        else
        {
            lcd_wr_regno(lcddev.setxcmd);
            lcd_wr_data(x >> 8);
            lcd_wr_data(x & 0XFF);
            lcd_wr_data((lcddev.width - 1) >> 8);
            lcd_wr_data((lcddev.width - 1) & 0XFF);
        }

        lcd_wr_regno(lcddev.setycmd);
        lcd_wr_data(y >> 8);
        lcd_wr_data(y & 0XFF);
        lcd_wr_data((lcddev.height - 1) >> 8);
        lcd_wr_data((lcddev.height - 1) & 0XFF);

    }
    else if (lcddev.id == 0X5510)
    {
        lcd_wr_regno(lcddev.setxcmd);
        lcd_wr_data(x >> 8);
        lcd_wr_regno(lcddev.setxcmd + 1);
        lcd_wr_data(x & 0XFF);
        lcd_wr_regno(lcddev.setycmd);
        lcd_wr_data(y >> 8);
        lcd_wr_regno(lcddev.setycmd + 1);
        lcd_wr_data(y & 0XFF);
    }
    else
    {
        lcd_wr_regno(lcddev.setxcmd);
        lcd_wr_data(x >> 8);
        lcd_wr_data(x & 0XFF);
        lcd_wr_regno(lcddev.setycmd);
        lcd_wr_data(y >> 8);
        lcd_wr_data(y & 0XFF);
    }
}

/**
 * @brief lcd_scan_dir：扫描或采样当前输入与设备状态，整理本轮可用数据。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param dir 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_scan_dir(uint8_t dir)
{
    uint16_t regval = 0;
    uint16_t dirreg = 0;
    uint16_t temp;

    if ((lcddev.dir == 1 && lcddev.id != 0X1963) || (lcddev.dir == 0 && lcddev.id == 0X1963))
    {
        switch (dir)
        {
            case 0:
                dir = 6;
                break;

            case 1:
                dir = 7;
                break;

            case 2:
                dir = 4;
                break;

            case 3:
                dir = 5;
                break;

            case 4:
                dir = 1;
                break;

            case 5:
                dir = 0;
                break;

            case 6:
                dir = 3;
                break;

            case 7:
                dir = 2;
                break;
        }
    }

    switch (dir)
    {
        case L2R_U2D:
            regval |= (0 << 7) | (0 << 6) | (0 << 5);
            break;

        case L2R_D2U:
            regval |= (1 << 7) | (0 << 6) | (0 << 5);
            break;

        case R2L_U2D:
            regval |= (0 << 7) | (1 << 6) | (0 << 5);
            break;

        case R2L_D2U:
            regval |= (1 << 7) | (1 << 6) | (0 << 5);
            break;

        case U2D_L2R:
            regval |= (0 << 7) | (0 << 6) | (1 << 5);
            break;

        case U2D_R2L:
            regval |= (0 << 7) | (1 << 6) | (1 << 5);
            break;

        case D2U_L2R:
            regval |= (1 << 7) | (0 << 6) | (1 << 5);
            break;

        case D2U_R2L:
            regval |= (1 << 7) | (1 << 6) | (1 << 5);
            break;
    }

    dirreg = 0X36;

    if (lcddev.id == 0X5510)
    {
        dirreg = 0X3600;
    }

    if (lcddev.id == 0X9341 || lcddev.id == 0X7789 || lcddev.id == 0X7796)
    {
        regval |= 0X08;
    }

    lcd_write_reg(dirreg, regval);

    if (lcddev.id != 0X1963)
    {
        if (regval & 0X20)
        {
            if (lcddev.width < lcddev.height)
            {
                temp = lcddev.width;
                lcddev.width = lcddev.height;
                lcddev.height = temp;
            }
        }
        else
        {
            if (lcddev.width > lcddev.height)
            {
                temp = lcddev.width;
                lcddev.width = lcddev.height;
                lcddev.height = temp;
            }
        }
    }

    if (lcddev.id == 0X5510)
    {
        lcd_wr_regno(lcddev.setxcmd);
        lcd_wr_data(0);
        lcd_wr_regno(lcddev.setxcmd + 1);
        lcd_wr_data(0);
        lcd_wr_regno(lcddev.setxcmd + 2);
        lcd_wr_data((lcddev.width - 1) >> 8);
        lcd_wr_regno(lcddev.setxcmd + 3);
        lcd_wr_data((lcddev.width - 1) & 0XFF);
        lcd_wr_regno(lcddev.setycmd);
        lcd_wr_data(0);
        lcd_wr_regno(lcddev.setycmd + 1);
        lcd_wr_data(0);
        lcd_wr_regno(lcddev.setycmd + 2);
        lcd_wr_data((lcddev.height - 1) >> 8);
        lcd_wr_regno(lcddev.setycmd + 3);
        lcd_wr_data((lcddev.height - 1) & 0XFF);
    }
    else
    {
        lcd_wr_regno(lcddev.setxcmd);
        lcd_wr_data(0);
        lcd_wr_data(0);
        lcd_wr_data((lcddev.width - 1) >> 8);
        lcd_wr_data((lcddev.width - 1) & 0XFF);
        lcd_wr_regno(lcddev.setycmd);
        lcd_wr_data(0);
        lcd_wr_data(0);
        lcd_wr_data((lcddev.height - 1) >> 8);
        lcd_wr_data((lcddev.height - 1) & 0XFF);
    }
}

/**
 * @brief lcd_draw_point：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_draw_point(uint16_t x, uint16_t y, uint32_t color)
{
    if (lcdltdc.pwidth != 0)
    {
        ltdc_draw_point(x, y, color);
    }else
    {
        lcd_set_cursor(x, y);
        lcd_write_ram_prepare();
        LCD->LCD_RAM = color;
    }
}

/**
 * @brief lcd_ssd_backlight_set：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param pwm 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_ssd_backlight_set(uint8_t pwm)
{
    lcd_wr_regno(0xBE);
    lcd_wr_data(0x05);
    lcd_wr_data(pwm * 2.55);
    lcd_wr_data(0x01);
    lcd_wr_data(0xFF);
    lcd_wr_data(0x00);
    lcd_wr_data(0x00);
}

/**
 * @brief lcd_display_dir：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param dir 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_display_dir(uint8_t dir)
{
    lcddev.dir = dir;

    if (lcdltdc.pwidth != 0)
    {
        ltdc_display_dir(dir);
        lcddev.width = lcdltdc.width;
        lcddev.height = lcdltdc.height;
        return;
    }

    if (dir == 0)
    {
        lcddev.width = 240;
        lcddev.height = 320;

        if (lcddev.id == 0x5510)
        {
            lcddev.wramcmd = 0X2C00;
            lcddev.setxcmd = 0X2A00;
            lcddev.setycmd = 0X2B00;
            lcddev.width = 480;
            lcddev.height = 800;
        }
        else if (lcddev.id == 0X1963)
        {
            lcddev.wramcmd = 0X2C;
            lcddev.setxcmd = 0X2B;
            lcddev.setycmd = 0X2A;
            lcddev.width = 480;
            lcddev.height = 800;
        }
        else
        {
            lcddev.wramcmd = 0X2C;
            lcddev.setxcmd = 0X2A;
            lcddev.setycmd = 0X2B;
        }

        if (lcddev.id == 0X5310 || lcddev.id == 0X7796)
        {
            lcddev.width = 320;
            lcddev.height = 480;
        }

        if (lcddev.id == 0X9806)
        {
            lcddev.width = 480;
            lcddev.height = 800;
        }
    }
    else
    {
        lcddev.width = 320;
        lcddev.height = 240;

        if (lcddev.id == 0x5510)
        {
            lcddev.wramcmd = 0X2C00;
            lcddev.setxcmd = 0X2A00;
            lcddev.setycmd = 0X2B00;
            lcddev.width = 800;
            lcddev.height = 480;
        }
        else if (lcddev.id == 0X1963 || lcddev.id == 0X9806)
        {
            lcddev.wramcmd = 0X2C;
            lcddev.setxcmd = 0X2A;
            lcddev.setycmd = 0X2B;
            lcddev.width = 800;
            lcddev.height = 480;
        }
        else
        {
            lcddev.wramcmd = 0X2C;
            lcddev.setxcmd = 0X2A;
            lcddev.setycmd = 0X2B;
        }

        if (lcddev.id == 0X5310 || lcddev.id == 0X7796)
        {
            lcddev.width = 480;
            lcddev.height = 320;
        }
    }

    lcd_scan_dir(DFT_SCAN_DIR);
}

/**
 * @brief lcd_set_window：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param sx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param sy 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param width 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param height 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_set_window(uint16_t sx, uint16_t sy, uint16_t width, uint16_t height)
{
    uint16_t twidth, theight;
    twidth = sx + width - 1;
    theight = sy + height - 1;

    if (lcdltdc.pwidth != 0)
    {
        return;
    }

    if (lcddev.id == 0X1963 && lcddev.dir != 1)
    {
        sx = lcddev.width - width - sx;
        height = sy + height - 1;
        lcd_wr_regno(lcddev.setxcmd);
        lcd_wr_data(sx >> 8);
        lcd_wr_data(sx & 0XFF);
        lcd_wr_data((sx + width - 1) >> 8);
        lcd_wr_data((sx + width - 1) & 0XFF);
        lcd_wr_regno(lcddev.setycmd);
        lcd_wr_data(sy >> 8);
        lcd_wr_data(sy & 0XFF);
        lcd_wr_data(height >> 8);
        lcd_wr_data(height & 0XFF);
    }
    else if (lcddev.id == 0X5510)
    {
        lcd_wr_regno(lcddev.setxcmd);
        lcd_wr_data(sx >> 8);
        lcd_wr_regno(lcddev.setxcmd + 1);
        lcd_wr_data(sx & 0XFF);
        lcd_wr_regno(lcddev.setxcmd + 2);
        lcd_wr_data(twidth >> 8);
        lcd_wr_regno(lcddev.setxcmd + 3);
        lcd_wr_data(twidth & 0XFF);
        lcd_wr_regno(lcddev.setycmd);
        lcd_wr_data(sy >> 8);
        lcd_wr_regno(lcddev.setycmd + 1);
        lcd_wr_data(sy & 0XFF);
        lcd_wr_regno(lcddev.setycmd + 2);
        lcd_wr_data(theight >> 8);
        lcd_wr_regno(lcddev.setycmd + 3);
        lcd_wr_data(theight & 0XFF);
    }
    else
    {
        lcd_wr_regno(lcddev.setxcmd);
        lcd_wr_data(sx >> 8);
        lcd_wr_data(sx & 0XFF);
        lcd_wr_data(twidth >> 8);
        lcd_wr_data(twidth & 0XFF);
        lcd_wr_regno(lcddev.setycmd);
        lcd_wr_data(sy >> 8);
        lcd_wr_data(sy & 0XFF);
        lcd_wr_data(theight >> 8);
        lcd_wr_data(theight & 0XFF);
    }
}

/**
 * @brief lcd_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void lcd_init(void)
{
    lcddev.id = ltdc_panelid_read();

    if (lcddev.id != 0)
    {
        ltdc_init();
    }
    else
    {
        LCD_CS_GPIO_CLK_ENABLE();
        LCD_WR_GPIO_CLK_ENABLE();
        LCD_RD_GPIO_CLK_ENABLE();
        LCD_RS_GPIO_CLK_ENABLE();
        LCD_BL_GPIO_CLK_ENABLE();

        RCC->AHB4ENR |= 3 << 3;
        RCC->AHB3ENR |= 1 << 12;

        sys_gpio_set(LCD_CS_GPIO_PORT, LCD_CS_GPIO_PIN,
                     SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);

        sys_gpio_set(LCD_WR_GPIO_PORT, LCD_WR_GPIO_PIN,
                     SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);

        sys_gpio_set(LCD_RD_GPIO_PORT, LCD_RD_GPIO_PIN,
                     SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);

        sys_gpio_set(LCD_RS_GPIO_PORT, LCD_RS_GPIO_PIN,
                     SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);

        sys_gpio_set(LCD_BL_GPIO_PORT, LCD_BL_GPIO_PIN,
                     SYS_GPIO_MODE_OUT, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);

        sys_gpio_af_set(LCD_CS_GPIO_PORT, LCD_CS_GPIO_PIN, 12);
        sys_gpio_af_set(LCD_WR_GPIO_PORT, LCD_WR_GPIO_PIN, 12);
        sys_gpio_af_set(LCD_RD_GPIO_PORT, LCD_RD_GPIO_PIN, 12);
        sys_gpio_af_set(LCD_RS_GPIO_PORT, LCD_RS_GPIO_PIN, 12);

        sys_gpio_set(GPIOD, (3 << 0) | (7 << 8) | (3 << 14), SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);
        sys_gpio_set(GPIOE, (0X1FF << 7), SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);

        sys_gpio_af_set(GPIOD, (3 << 0) | (7 << 8) | (3 << 14), 12);
        sys_gpio_af_set(GPIOE, (0X1FF << 7), 12);

        LCD_FMC_BCRX = 0X00000000;
        LCD_FMC_BTRX = 0X00000000;
        LCD_FMC_BWTRX = 0X00000000;

        LCD_FMC_BCRX |= 1 << 12;
        LCD_FMC_BCRX |= 1 << 14;
        LCD_FMC_BCRX |= 1 << 4;

        LCD_FMC_BTRX |= 0 << 28;
        LCD_FMC_BTRX |= 15 << 0;

        LCD_FMC_BTRX |= 78 << 8;

        LCD_FMC_BWTRX |= 0 << 28;
        LCD_FMC_BWTRX |= 15 << 0;

        LCD_FMC_BWTRX |= 15 << 8;

        LCD_FMC_BCRX |= 1 << 0;
        LCD_FMC_BCRX |= (uint32_t)1 << 31;

        lcd_opt_delay(0XFFFFF);

        lcd_wr_regno(0XD3);
        lcddev.id = lcd_rd_data();
        lcddev.id = lcd_rd_data();
        lcddev.id = lcd_rd_data();
        lcddev.id <<= 8;
        lcddev.id |= lcd_rd_data();

        if (lcddev.id != 0X9341)
        {
            lcd_wr_regno(0X04);
            lcddev.id = lcd_rd_data();
            lcddev.id = lcd_rd_data();
            lcddev.id = lcd_rd_data();
            lcddev.id <<= 8;
            lcddev.id |= lcd_rd_data();

            if (lcddev.id == 0X8552)
            {
                lcddev.id = 0x7789;
            }

            if (lcddev.id != 0x7789)
            {
                lcd_wr_regno(0XD4);
                lcddev.id = lcd_rd_data();
                lcddev.id = lcd_rd_data();
                lcddev.id = lcd_rd_data();
                lcddev.id <<= 8;
                lcddev.id |= lcd_rd_data();

                if (lcddev.id != 0X5310)
                {
                    lcd_wr_regno(0XD3);
                    lcddev.id = lcd_rd_data();
                    lcddev.id = lcd_rd_data();
                    lcddev.id = lcd_rd_data();
                    lcddev.id <<= 8;
                    lcddev.id |= lcd_rd_data();

                    if (lcddev.id != 0x7796)
                    {

                        lcd_write_reg(0xF000, 0x0055);
                        lcd_write_reg(0xF001, 0x00AA);
                        lcd_write_reg(0xF002, 0x0052);
                        lcd_write_reg(0xF003, 0x0008);
                        lcd_write_reg(0xF004, 0x0001);

                        lcd_wr_regno(0xC500);
                        lcddev.id = lcd_rd_data();
                        lcddev.id <<= 8;

                        lcd_wr_regno(0xC501);
                        lcddev.id |= lcd_rd_data();

                        delay_ms(5);

                        if (lcddev.id != 0X5510)
                        {
                            lcd_wr_regno(0XD3);
                            lcddev.id = lcd_rd_data();
                            lcddev.id = lcd_rd_data();
                            lcddev.id = lcd_rd_data();
                            lcddev.id <<= 8;
                            lcddev.id |= lcd_rd_data();

                            if (lcddev.id != 0x9806)
                            {
                                lcd_wr_regno(0XA1);
                                lcddev.id = lcd_rd_data();
                                lcddev.id = lcd_rd_data();
                                lcddev.id <<= 8;
                                lcddev.id |= lcd_rd_data();

                                if (lcddev.id == 0X5761) lcddev.id = 0X1963;
                            }
                        }
                    }
                }
            }
        }
    }

    printf("LCD ID:%x\r\n", lcddev.id);

    if (lcddev.id == 0X7789)
    {
        lcd_ex_st7789_reginit();
    }
    else if (lcddev.id == 0X9341)
    {
        lcd_ex_ili9341_reginit();
    }
    else if (lcddev.id == 0x5310)
    {
        lcd_ex_nt35310_reginit();
    }
    else if (lcddev.id == 0x7796)
    {
        lcd_ex_st7796_reginit();
    }
    else if (lcddev.id == 0x5510)
    {
        lcd_ex_nt35510_reginit();
    }
    else if (lcddev.id == 0x9806)
    {
        lcd_ex_ili9806_reginit();
    }
    else if (lcddev.id == 0X1963)
    {
        lcd_ex_ssd1963_reginit();
        lcd_ssd_backlight_set(100);
    }

    if (lcddev.id == 0X7789)
    {

        LCD_FMC_BWTRX &= ~(0XF << 0);
        LCD_FMC_BWTRX &= ~(0XF << 8);
        LCD_FMC_BWTRX |= 5 << 0;
        LCD_FMC_BWTRX |= 5 << 8;
    }
    else if (lcddev.id == 0X1963)
    {

        LCD_FMC_BWTRX &= ~(0XF << 0);
        LCD_FMC_BWTRX &= ~(0XF << 8);
        LCD_FMC_BWTRX |= 7 << 0;
        LCD_FMC_BWTRX |= 7 << 8;
    }
    else if (lcddev.id == 0X5510 || lcddev.id == 0X9806)
    {

        LCD_FMC_BWTRX &= ~(0XF << 0);
        LCD_FMC_BWTRX &= ~(0XF << 8);
        LCD_FMC_BWTRX |= 2 << 0;
        LCD_FMC_BWTRX |= 2 << 8;
    }
    else if (lcddev.id == 0X5310 || lcddev.id == 0X7796 || lcddev.id == 0X9341)
    {

        LCD_FMC_BWTRX &= ~(0XF << 0);
        LCD_FMC_BWTRX &= ~(0XF << 8);
        LCD_FMC_BWTRX |= 3 << 0;
        LCD_FMC_BWTRX |= 3 << 8;
    }

    lcd_display_dir(0);
    LCD_BL(1);
    lcd_clear(WHITE);
}

/**
 * @brief lcd_clear：清除已有状态或复位目标设备，使其回到约定的初始状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_clear(uint16_t color)
{
    uint32_t index = 0;
    uint32_t totalpoint = lcddev.width;

    if (lcdltdc.pwidth != 0)
    {
        ltdc_clear(color);
    }
    else
    {
        totalpoint *= lcddev.height;
        lcd_set_cursor(0x00, 0x0000);
        lcd_write_ram_prepare();

        for (index = 0; index < totalpoint; index++)
        {
            LCD->LCD_RAM = color;
       }
   }
}

/**
 * @brief lcd_fill：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param sx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param sy 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param ex 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param ey 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint32_t color)
{
    uint16_t i, j;
    uint16_t xlen = 0;

    if (lcdltdc.pwidth != 0)
    {
        ltdc_fill(sx, sy, ex, ey, color);
    }
    else
    {
        xlen = ex - sx + 1;
        for (i = sy; i <= ey; i++)
        {
            lcd_set_cursor(sx, i);
            lcd_write_ram_prepare();

            for (j = 0; j < xlen; j++)
            {
                LCD->LCD_RAM = color;
            }
        }
    }
}

/**
 * @brief lcd_color_fill：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param sx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param sy 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param ex 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param ey 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_color_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color)
{
    uint16_t height, width;
    uint16_t i, j;

    if (lcdltdc.pwidth != 0)
    {
        ltdc_color_fill(sx, sy, ex, ey, color);
    }
    else
    {
        width = ex - sx + 1;
        height = ey - sy + 1;

        for (i = 0; i < height; i++)
        {
            lcd_set_cursor(sx, sy + i);
            lcd_write_ram_prepare();

            for (j = 0; j < width; j++)
            {
                LCD->LCD_RAM = color[i * width + j];
            }
        }
    }
}

/**
 * @brief lcd_draw_line：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x1 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y1 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param x2 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y2 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_draw_line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    uint16_t t;
    int xerr = 0, yerr = 0, delta_x, delta_y, distance;
    int incx, incy, row, col;
    delta_x = x2 - x1;
    delta_y = y2 - y1;
    row = x1;
    col = y1;

    if (delta_x > 0)incx = 1;
    else if (delta_x == 0)incx = 0;
    else
    {
        incx = -1;
        delta_x = -delta_x;
    }

    if (delta_y > 0)incy = 1;
    else if (delta_y == 0)incy = 0;
    else
    {
        incy = -1;
        delta_y = -delta_y;
    }

    if ( delta_x > delta_y)distance = delta_x;
    else distance = delta_y;

    for (t = 0; t <= distance + 1; t++ )
    {
        lcd_draw_point(row, col, color);
        xerr += delta_x ;
        yerr += delta_y ;

        if (xerr > distance)
        {
            xerr -= distance;
            row += incx;
        }

        if (yerr > distance)
        {
            yerr -= distance;
            col += incy;
        }
    }
}

/**
 * @brief lcd_draw_hline：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param len 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_draw_hline(uint16_t x, uint16_t y, uint16_t len, uint16_t color)
{
    if ((len == 0) || (x > lcddev.width) || (y > lcddev.height))return;

    lcd_fill(x, y, x + len - 1, y, color);
}

/**
 * @brief lcd_draw_rectangle：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x1 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y1 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param x2 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y2 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_draw_rectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    lcd_draw_line(x1, y1, x2, y1, color);
    lcd_draw_line(x1, y1, x1, y2, color);
    lcd_draw_line(x1, y2, x2, y2, color);
    lcd_draw_line(x2, y1, x2, y2, color);
}

/**
 * @brief lcd_draw_circle：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x0 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y0 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param r 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_draw_circle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color)
{
    int a, b;
    int di;
    a = 0;
    b = r;
    di = 3 - (r << 1);

    while (a <= b)
    {
        lcd_draw_point(x0 + a, y0 - b, color);
        lcd_draw_point(x0 + b, y0 - a, color);
        lcd_draw_point(x0 + b, y0 + a, color);
        lcd_draw_point(x0 + a, y0 + b, color);
        lcd_draw_point(x0 - a, y0 + b, color);
        lcd_draw_point(x0 - b, y0 + a, color);
        lcd_draw_point(x0 - a, y0 - b, color);
        lcd_draw_point(x0 - b, y0 - a, color);
        a++;

        if (di < 0)
        {
            di += 4 * a + 6;
        }
        else
        {
            di += 10 + 4 * (a - b);
            b--;
        }
    }
}

/**
 * @brief lcd_fill_circle：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param r 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_fill_circle(uint16_t x, uint16_t y, uint16_t r, uint16_t color)
{
    uint32_t i;
    uint32_t imax = ((uint32_t)r * 707) / 1000 + 1;
    uint32_t sqmax = (uint32_t)r * (uint32_t)r + (uint32_t)r / 2;
    uint32_t xr = r;

    lcd_draw_hline(x - r, y, 2 * r, color);

    for (i = 1; i <= imax; i++)
    {
        if ((i * i + xr * xr) > sqmax)
        {

            if (xr > imax)
            {
                lcd_draw_hline (x - i + 1, y + xr, 2 * (i - 1), color);
                lcd_draw_hline (x - i + 1, y - xr, 2 * (i - 1), color);
            }

            xr--;
        }

        lcd_draw_hline(x - xr, y + i, 2 * xr, color);
        lcd_draw_hline(x - xr, y - i, 2 * xr, color);
    }
}

/**
 * @brief lcd_show_char：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param chr 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param size 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param mode 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_show_char(uint16_t x, uint16_t y, char chr, uint8_t size, uint8_t mode, uint16_t color)
{
    uint8_t temp, t1, t;
    uint16_t y0 = y;
    uint8_t csize = 0;
    uint8_t *pfont = 0;

    csize = (size / 8 + ((size % 8) ? 1 : 0)) * (size / 2);
    chr = chr - ' ';

    switch (size)
    {
        case 12:
            pfont = (uint8_t *)asc2_1206[chr];
            break;

        case 16:
            pfont = (uint8_t *)asc2_1608[chr];
            break;

        case 24:
            pfont = (uint8_t *)asc2_2412[chr];
            break;

        case 32:
            pfont = (uint8_t *)asc2_3216[chr];
            break;

        default:
            return ;
    }

    for (t = 0; t < csize; t++)
    {
        temp = pfont[t];

        for (t1 = 0; t1 < 8; t1++)
        {
            if (temp & 0x80)
            {
                lcd_draw_point(x, y, color);
            }
            else if (mode == 0)
            {
                lcd_draw_point(x, y, g_back_color);
            }

            temp <<= 1;
            y++;

            if (y >= lcddev.height)return;

            if ((y - y0) == size)
            {
                y = y0;
                x++;

                if (x >= lcddev.width)return;

                break;
            }
        }
    }
}

/**
 * @brief lcd_pow：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param m 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param n 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint32_t lcd_pow(uint8_t m, uint8_t n)
{
    uint32_t result = 1;

    while (n--)result *= m;

    return result;
}

/**
 * @brief lcd_show_num：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param num 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param len 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param size 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void lcd_show_num(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint16_t color)
{
    uint8_t t, temp;
    uint8_t enshow = 0;

    for (t = 0; t < len; t++)
    {
        temp = (num / lcd_pow(10, len - t - 1)) % 10;

        if (enshow == 0 && t < (len - 1))
        {
            if (temp == 0)
            {
                lcd_show_char(x + (size / 2)*t, y, ' ', size, 0, color);
                continue;
            }
            else
            {
                enshow = 1;
            }

        }

        lcd_show_char(x + (size / 2)*t, y, temp + '0', size, 0, color);
    }
}

/**
 * @brief lcd_show_xnum：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
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
void lcd_show_xnum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint8_t size, uint8_t mode, uint16_t color)
{
    uint8_t t, temp;
    uint8_t enshow = 0;

    for (t = 0; t < len; t++)
    {
        temp = (num / lcd_pow(10, len - t - 1)) % 10;

        if (enshow == 0 && t < (len - 1))
        {
            if (temp == 0)
            {
                if (mode & 0X80)
                {
                    lcd_show_char(x + (size / 2)*t, y, '0', size, mode & 0X01, color);
                }
                else
                {
                    lcd_show_char(x + (size / 2)*t, y, ' ', size, mode & 0X01, color);
                }

                continue;
            }
            else
            {
                enshow = 1;
            }

        }

        lcd_show_char(x + (size / 2)*t, y, temp + '0', size, mode & 0X01, color);
    }
}

/**
 * @brief lcd_show_string：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
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
void lcd_show_string(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint8_t size, char *p, uint16_t color)
{
    uint8_t x0 = x;
    width += x;
    height += y;

    while ((*p <= '~') && (*p >= ' '))
    {
        if (x >= width)
        {
            x = x0;
            y += size;
        }

        if (y >= height)break;

        lcd_show_char(x, y, *p, size, 0, color);
        x += size / 2;
        p++;
    }
}
