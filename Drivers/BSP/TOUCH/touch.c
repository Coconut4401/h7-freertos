/**
 * @file touch.c
 * @brief 统一电阻屏与电容屏初始化、校准、扫描和坐标状态。
 * @details 这是 touch 模块的实现文件（Drivers/BSP/TOUCH/touch.c）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#include "stdio.h"
#include "stdlib.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/TOUCH/touch.h"
#include "./BSP/24CXX/24cxx.h"
#include "./SYSTEM/delay/delay.h"

_m_tp_dev tp_dev =
{
    tp_init,
    tp_scan,
    tp_adjust,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
};

/**
 * @brief tp_write_byte：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param data 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void tp_write_byte(uint8_t data)
{
    uint8_t count = 0;

    for (count = 0; count < 8; count++)
    {
        if (data & 0x80)
        {
            T_MOSI(1);
        }
        else
        {
            T_MOSI(0);
        }

        data <<= 1;
        T_CLK(0);
        delay_us(1);
        T_CLK(1);
    }
}

/**
 * @brief tp_read_ad：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param cmd 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint16_t tp_read_ad(uint8_t cmd)
{
    uint8_t count = 0;
    uint16_t num = 0;
    T_CLK(0);
    T_MOSI(0);
    T_CS(0);
    tp_write_byte(cmd);
    delay_us(6);
    T_CLK(0);
    delay_us(1);
    T_CLK(1);
    delay_us(1);
    T_CLK(0);

    for (count = 0; count < 16; count++)
    {
        num <<= 1;
        T_CLK(0);
        delay_us(1);
        T_CLK(1);

        if (T_MISO)num++;
    }

    num >>= 4;
    T_CS(1);
    return num;
}

/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define TP_READ_TIMES   5
#define TP_LOST_VAL     1

/**
 * @brief tp_read_xoy：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param cmd 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint16_t tp_read_xoy(uint8_t cmd)
{
    uint16_t i, j;
    uint16_t buf[TP_READ_TIMES];
    uint16_t sum = 0;
    uint16_t temp;

    for (i = 0; i < TP_READ_TIMES; i++)
    {
        buf[i] = tp_read_ad(cmd);
    }

    for (i = 0; i < TP_READ_TIMES - 1; i++)
    {
        for (j = i + 1; j < TP_READ_TIMES; j++)
        {
            if (buf[i] > buf[j])
            {
                temp = buf[i];
                buf[i] = buf[j];
                buf[j] = temp;
            }
        }
    }

    sum = 0;

    for (i = TP_LOST_VAL; i < TP_READ_TIMES - TP_LOST_VAL; i++)
    {
        sum += buf[i];
    }

    temp = sum / (TP_READ_TIMES - 2 * TP_LOST_VAL);
    return temp;
}

/**
 * @brief tp_read_xy：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void tp_read_xy(uint16_t *x, uint16_t *y)
{
    uint16_t xval, yval;

    if (tp_dev.touchtype & 0X01)
    {
        xval = tp_read_xoy(0X90);
        yval = tp_read_xoy(0XD0);
    }
    else
    {
        xval = tp_read_xoy(0XD0);
        yval = tp_read_xoy(0X90);
    }

    *x = xval;
    *y = yval;
}

#define TP_ERR_RANGE    50

/**
 * @brief tp_read_xy2：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint8_t tp_read_xy2(uint16_t *x, uint16_t *y)
{
    uint16_t x1, y1;
    uint16_t x2, y2;

    tp_read_xy(&x1, &y1);
    tp_read_xy(&x2, &y2);

    if (((x2 <= x1 && x1 < x2 + TP_ERR_RANGE) || (x1 <= x2 && x2 < x1 + TP_ERR_RANGE)) &&
            ((y2 <= y1 && y1 < y2 + TP_ERR_RANGE) || (y1 <= y2 && y2 < y1 + TP_ERR_RANGE)))
    {
        *x = (x1 + x2) / 2;
        *y = (y1 + y2) / 2;
        return 1;
    }

    return 0;
}

/**
 * @brief tp_draw_touch_point：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void tp_draw_touch_point(uint16_t x, uint16_t y, uint16_t color)
{
    lcd_draw_line(x - 12, y, x + 13, y, color);
    lcd_draw_line(x, y - 12, x, y + 13, color);
    lcd_draw_point(x + 1, y + 1, color);
    lcd_draw_point(x - 1, y + 1, color);
    lcd_draw_point(x + 1, y - 1, color);
    lcd_draw_point(x - 1, y - 1, color);
    lcd_draw_circle(x, y, 6, color);
}

/**
 * @brief tp_draw_big_point：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void tp_draw_big_point(uint16_t x, uint16_t y, uint16_t color)
{
    lcd_draw_point(x, y, color);
    lcd_draw_point(x + 1, y, color);
    lcd_draw_point(x, y + 1, color);
    lcd_draw_point(x + 1, y + 1, color);
}

/**
 * @brief tp_scan：扫描或采样当前输入与设备状态，整理本轮可用数据。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param mode 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t tp_scan(uint8_t mode)
{
    if (T_PEN == 0)
    {
        if (mode)
        {
            tp_read_xy2(&tp_dev.x[0], &tp_dev.y[0]);
        }
        else if (tp_read_xy2(&tp_dev.x[0], &tp_dev.y[0]))
        {

            tp_dev.x[0] = (signed short)(tp_dev.x[0] - tp_dev.xc) / tp_dev.xfac + lcddev.width / 2;

            tp_dev.y[0] = (signed short)(tp_dev.y[0] - tp_dev.yc) / tp_dev.yfac + lcddev.height / 2;
        }

        if ((tp_dev.sta & TP_PRES_DOWN) == 0)
        {
            tp_dev.sta = TP_PRES_DOWN | TP_CATH_PRES;
            tp_dev.x[CT_MAX_TOUCH - 1] = tp_dev.x[0];
            tp_dev.y[CT_MAX_TOUCH - 1] = tp_dev.y[0];
        }
    }
    else
    {
        if (tp_dev.sta & TP_PRES_DOWN)
        {
            tp_dev.sta &= ~TP_PRES_DOWN;
        }
        else
        {
            tp_dev.x[CT_MAX_TOUCH - 1] = 0;
            tp_dev.y[CT_MAX_TOUCH - 1] = 0;
            tp_dev.x[0] = 0xffff;
            tp_dev.y[0] = 0xffff;
        }
    }

    return tp_dev.sta & TP_PRES_DOWN;
}

#define TP_SAVE_ADDR_BASE   40

/**
 * @brief tp_save_adjust_data：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void tp_save_adjust_data(void)
{
    uint8_t *p = (uint8_t *)&tp_dev.xfac;

    at24cxx_write(TP_SAVE_ADDR_BASE, p, 12);
    at24cxx_write_one_byte(TP_SAVE_ADDR_BASE + 12, 0X0A);
}

/**
 * @brief tp_get_adjust_data：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t tp_get_adjust_data(void)
{
    uint8_t *p = (uint8_t *)&tp_dev.xfac;
    uint8_t temp = 0;

    at24cxx_read(TP_SAVE_ADDR_BASE, p, 12);
    temp = at24cxx_read_one_byte(TP_SAVE_ADDR_BASE + 12);

    if (temp == 0X0A)
    {
        return 1;
    }

    return 0;
}

char *const TP_REMIND_MSG_TBL = "Please use the stylus click the cross on the screen.The cross will always move until the screen adjustment is completed.";

/**
 * @brief tp_adjust_info_show：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param px 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param py 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void tp_adjust_info_show(uint16_t xy[5][2], double px, double py)
{
    uint8_t i;
    char sbuf[20];

    for (i = 0; i < 5; i++)
    {
        sprintf(sbuf, "x%d:%d", i + 1, xy[i][0]);
        lcd_show_string(40, 160 + (i * 20), lcddev.width, lcddev.height, 16, sbuf, RED);
        sprintf(sbuf, "y%d:%d", i + 1, xy[i][1]);
        lcd_show_string(40 + 80, 160 + (i * 20), lcddev.width, lcddev.height, 16, sbuf, RED);
    }

    lcd_fill(40, 160 + (i * 20), lcddev.width - 1, 16, WHITE);
    sprintf(sbuf, "px:%0.2f", px);
    sbuf[7] = 0;
    lcd_show_string(40, 160 + (i * 20), lcddev.width, lcddev.height, 16, sbuf, RED);
    sprintf(sbuf, "py:%0.2f", py);
    sbuf[7] = 0;
    lcd_show_string(40 + 80, 160 + (i * 20), lcddev.width, lcddev.height, 16, sbuf, RED);
}

/**
 * @brief tp_adjust：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void tp_adjust(void)
{
    uint16_t pxy[5][2];
    uint8_t  cnt = 0;
    short s1, s2, s3, s4;
    double px, py;
    uint16_t outtime = 0;
    cnt = 0;

    lcd_clear(WHITE);
    lcd_show_string(40, 40, 160, 100, 16, TP_REMIND_MSG_TBL, RED);
    tp_draw_touch_point(20, 20, RED);
    tp_dev.sta = 0;

    while (1)
    {
        tp_dev.scan(1);

        if ((tp_dev.sta & 0xc000) == TP_CATH_PRES)
        {
            outtime = 0;
            tp_dev.sta &= ~TP_CATH_PRES;

            pxy[cnt][0] = tp_dev.x[0];
            pxy[cnt][1] = tp_dev.y[0];
            cnt++;

            switch (cnt)
            {
                case 1:
                    tp_draw_touch_point(20, 20, WHITE);
                    tp_draw_touch_point(lcddev.width - 20, 20, RED);
                    break;

                case 2:
                    tp_draw_touch_point(lcddev.width - 20, 20, WHITE);
                    tp_draw_touch_point(20, lcddev.height - 20, RED);
                    break;

                case 3:
                    tp_draw_touch_point(20, lcddev.height - 20, WHITE);
                    tp_draw_touch_point(lcddev.width - 20, lcddev.height - 20, RED);
                    break;

                case 4:
                    lcd_clear(WHITE);
                    tp_draw_touch_point(lcddev.width / 2, lcddev.height / 2, RED);
                    break;

                case 5:
                    s1 = pxy[1][0] - pxy[0][0];
                    s3 = pxy[3][0] - pxy[2][0];
                    s2 = pxy[3][1] - pxy[1][1];
                    s4 = pxy[2][1] - pxy[0][1];

                    px = (double)s1 / s3;
                    py = (double)s2 / s4;

                    if (px < 0)px = -px;
                    if (py < 0)py = -py;

                    if (px < 0.95 || px > 1.05 || py < 0.95 || py > 1.05 ||
                            abs(s1) > 4095 || abs(s2) > 4095 || abs(s3) > 4095 || abs(s4) > 4095 ||
                            abs(s1) == 0 || abs(s2) == 0 || abs(s3) == 0 || abs(s4) == 0
                       )
                    {
                        cnt = 0;
                        tp_draw_touch_point(lcddev.width / 2, lcddev.height / 2, WHITE);
                        tp_draw_touch_point(20, 20, RED);
                        tp_adjust_info_show(pxy, px, py);
                        continue;
                    }

                    tp_dev.xfac = (float)(s1 + s3) / (2 * (lcddev.width - 40));
                    tp_dev.yfac = (float)(s2 + s4) / (2 * (lcddev.height - 40));

                    tp_dev.xc = pxy[4][0];
                    tp_dev.yc = pxy[4][1];

                    lcd_clear(WHITE);
                    lcd_show_string(35, 110, lcddev.width, lcddev.height, 16, "Touch Screen Adjust OK!", BLUE);
                    delay_ms(1000);
                    tp_save_adjust_data();

                    lcd_clear(WHITE);
                    return;
            }
        }

        delay_ms(10);
        outtime++;

        if (outtime > 1000)
        {
            tp_get_adjust_data();
            break;
        }
    }

}

/**
 * @brief tp_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t tp_init(void)
{
    tp_dev.touchtype = 0;
    tp_dev.touchtype |= lcddev.dir & 0X01;

    if (lcddev.id == 0x7796)
    {
        if (gt9xxx_init() == 0)
        {
            tp_dev.scan = gt9xxx_scan;
            tp_dev.touchtype |= 0X80;
            return 0;
        }
    }
    if (lcddev.id == 0X5510 || lcddev.id == 0X4342 || lcddev.id == 0X1018  || lcddev.id == 0X4384 || lcddev.id == 0X9806)
    {
        gt9xxx_init();
        tp_dev.scan = gt9xxx_scan;
        tp_dev.touchtype |= 0X80;
        return 0;
    }
    else if (lcddev.id == 0X1963 || lcddev.id == 0X7084 || lcddev.id == 0X7016)
    {
        if (!ft5206_init())
        {
            tp_dev.scan = ft5206_scan;
        }
        else
        {
            gt9xxx_init();
            tp_dev.scan = gt9xxx_scan;
        }
        tp_dev.touchtype |= 0X80;
        return 0;
    }
    else
    {
        T_PEN_GPIO_CLK_ENABLE();
        T_CS_GPIO_CLK_ENABLE();
        T_MISO_GPIO_CLK_ENABLE();
        T_MOSI_GPIO_CLK_ENABLE();
        T_CLK_GPIO_CLK_ENABLE();

        sys_gpio_set(T_PEN_GPIO_PORT, T_PEN_GPIO_PIN,
                     SYS_GPIO_MODE_IN, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_MID, SYS_GPIO_PUPD_PU);

        sys_gpio_set(T_MISO_GPIO_PORT, T_MISO_GPIO_PIN,
                     SYS_GPIO_MODE_IN, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_MID, SYS_GPIO_PUPD_PU);

        sys_gpio_set(T_MOSI_GPIO_PORT, T_MOSI_GPIO_PIN,
                     SYS_GPIO_MODE_OUT, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_MID, SYS_GPIO_PUPD_PU);

        sys_gpio_set(T_CLK_GPIO_PORT, T_CLK_GPIO_PIN,
                     SYS_GPIO_MODE_OUT, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_MID, SYS_GPIO_PUPD_PU);

        sys_gpio_set(T_CS_GPIO_PORT, T_CS_GPIO_PIN,
                     SYS_GPIO_MODE_OUT, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_MID, SYS_GPIO_PUPD_PU);

        tp_read_xy(&tp_dev.x[0], &tp_dev.y[0]);
        at24cxx_init();

        if (tp_get_adjust_data())
        {
            return 0;
        }
        else
        {
            lcd_clear(WHITE);
            tp_adjust();
            tp_save_adjust_data();
        }

        tp_get_adjust_data();
    }

    return 1;
}
