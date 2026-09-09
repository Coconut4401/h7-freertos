/**
 * @file gt9xxx.c
 * @brief 驱动 GT9xxx 电容触摸控制器，完成地址探测和触点读取。
 * @details 这是 gt9xxx 模块的实现文件（Drivers/BSP/TOUCH/gt9xxx.c）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#include "string.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/TOUCH/touch.h"
#include "./BSP/TOUCH/ctiic.h"
#include "./BSP/TOUCH/gt9xxx.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"

uint8_t g_gt_tnum = 5;
static uint8_t g_gt_cmd_wr = GT9XXX_ADDR_14_WR;
static uint8_t g_gt_cmd_rd = GT9XXX_ADDR_14_RD;

/**
 * @brief gt9xxx_pid_valid：检查函数名所描述的条件是否成立，并返回明确的判断结果。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param pid 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint8_t gt9xxx_pid_valid(const uint8_t *pid)
{
    uint8_t index;
    uint8_t length = 0U;

    for (index = 0U; index < 4U; index++)
    {
        if (pid[index] == 0U)
        {
            break;
        }

        if (pid[index] < 0x20U || pid[index] > 0x7EU)
        {
            return 0U;
        }
        length++;
    }

    return length >= 3U;
}

/**
 * @brief gt9xxx_reset_and_probe：执行设备探测或自检，并将检测结果返回或记录给上层模块。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param int_level 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param write_address 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint8_t gt9xxx_reset_and_probe(uint8_t int_level, uint8_t write_address)
{
    sys_gpio_set(GT9XXX_INT_GPIO_PORT, GT9XXX_INT_GPIO_PIN,
                 SYS_GPIO_MODE_OUT, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_MID, SYS_GPIO_PUPD_NONE);

    GT9XXX_RST(0);
    delay_ms(10);
    sys_gpio_pin_set(GT9XXX_INT_GPIO_PORT, GT9XXX_INT_GPIO_PIN, int_level);
    delay_ms(1);
    GT9XXX_RST(1);
    delay_ms(6);

    sys_gpio_set(GT9XXX_INT_GPIO_PORT, GT9XXX_INT_GPIO_PIN,
                 SYS_GPIO_MODE_IN, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_MID, SYS_GPIO_PUPD_NONE);
    delay_ms(50);

    return ct_iic_check_device(write_address);
}

/**
 * @brief gt9xxx_get_i2c_address：返回当前探测成功的 GT9xxx 七位 I2C 地址。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t gt9xxx_get_i2c_address(void)
{
    return g_gt_cmd_wr >> 1;
}

/**
 * @brief gt9xxx_wr_reg：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param reg 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param buf 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param len 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t gt9xxx_wr_reg(uint16_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t i;
    uint8_t ret = 0;
    ct_iic_start();
    ct_iic_send_byte(g_gt_cmd_wr);
    ct_iic_wait_ack();
    ct_iic_send_byte(reg >> 8);
    ct_iic_wait_ack();
    ct_iic_send_byte(reg & 0XFF);
    ct_iic_wait_ack();

    for (i = 0; i < len; i++)
    {
        ct_iic_send_byte(buf[i]);
        ret = ct_iic_wait_ack();

        if (ret)break;
    }

    ct_iic_stop();
    return ret;
}

/**
 * @brief gt9xxx_rd_reg：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param reg 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param buf 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param len 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void gt9xxx_rd_reg(uint16_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t i;
    ct_iic_start();
    ct_iic_send_byte(g_gt_cmd_wr);
    ct_iic_wait_ack();
    ct_iic_send_byte(reg >> 8);
    ct_iic_wait_ack();
    ct_iic_send_byte(reg & 0XFF);
    ct_iic_wait_ack();
    ct_iic_start();
    ct_iic_send_byte(g_gt_cmd_rd);
    ct_iic_wait_ack();

    for (i = 0; i < len; i++)
    {
        buf[i] = ct_iic_read_byte(i == (len - 1) ? 0 : 1);
    }

    ct_iic_stop();
}

/**
 * @brief gt9xxx_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t gt9xxx_init(void)
{
    uint8_t temp[5];

    GT9XXX_RST_GPIO_CLK_ENABLE();
    GT9XXX_INT_GPIO_CLK_ENABLE();

    sys_gpio_set(GT9XXX_RST_GPIO_PORT, GT9XXX_RST_GPIO_PIN,
                 SYS_GPIO_MODE_OUT, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_MID, SYS_GPIO_PUPD_PU);

    ct_iic_init();

    if (gt9xxx_reset_and_probe(1U, GT9XXX_ADDR_14_WR) == 0U)
    {
        g_gt_cmd_wr = GT9XXX_ADDR_14_WR;
        g_gt_cmd_rd = GT9XXX_ADDR_14_RD;
        printf("GT9xxx I2C address 0x14: ACK\r\n");
    }
    else
    {
        printf("GT9xxx I2C address 0x14: NACK\r\n");

        if (gt9xxx_reset_and_probe(0U, GT9XXX_ADDR_5D_WR) != 0U)
        {
            printf("GT9xxx I2C address 0x5D: NACK\r\n");
            return 1;
        }

        g_gt_cmd_wr = GT9XXX_ADDR_5D_WR;
        g_gt_cmd_rd = GT9XXX_ADDR_5D_RD;
        printf("GT9xxx I2C address 0x5D: ACK\r\n");
    }

    gt9xxx_rd_reg(GT9XXX_PID_REG, temp, 4);
    temp[4] = 0;
    printf("GT9xxx PID raw: %02X %02X %02X %02X, text: %s\r\n",
           temp[0], temp[1], temp[2], temp[3], temp);

    if (!gt9xxx_pid_valid(temp))
    {
        printf("GT9xxx product ID is not valid ASCII\r\n");
        return 1;
    }
    printf("CTP ID:%s\r\n", temp);

    if (strcmp((char *)temp, "9271") == 0)
    {
         g_gt_tnum = 10;
    }

    temp[0] = 0X02;
    gt9xxx_wr_reg(GT9XXX_CTRL_REG, temp, 1);

    delay_ms(10);

    temp[0] = 0X00;
    gt9xxx_wr_reg(GT9XXX_CTRL_REG, temp, 1);

    return 0;
}

const uint16_t GT9XXX_TPX_TBL[10] =
{
    GT9XXX_TP1_REG, GT9XXX_TP2_REG, GT9XXX_TP3_REG, GT9XXX_TP4_REG, GT9XXX_TP5_REG,
    GT9XXX_TP6_REG, GT9XXX_TP7_REG, GT9XXX_TP8_REG, GT9XXX_TP9_REG, GT9XXX_TP10_REG,
};

/**
 * @brief gt9xxx_scan：扫描或采样当前输入与设备状态，整理本轮可用数据。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param mode 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t gt9xxx_scan(uint8_t mode)
{
    uint8_t buf[4];
    uint8_t i = 0;
    uint8_t res = 0;
    uint16_t temp;
    uint16_t tempsta;
    static uint8_t t = 0;
    t++;

    if ((t % 10) == 0 || t < 10)
    {
        gt9xxx_rd_reg(GT9XXX_GSTID_REG, &mode, 1);

        if ((mode & 0X80) && ((mode & 0XF) <= g_gt_tnum))
        {
            i = 0;
            gt9xxx_wr_reg(GT9XXX_GSTID_REG, &i, 1);
        }

        if ((mode & 0XF) && ((mode & 0XF) <= g_gt_tnum))
        {
            temp = 0XFFFF << (mode & 0XF);
            tempsta = tp_dev.sta;
            tp_dev.sta = (~temp) | TP_PRES_DOWN | TP_CATH_PRES;
            tp_dev.x[g_gt_tnum - 1] = tp_dev.x[0];
            tp_dev.y[g_gt_tnum - 1] = tp_dev.y[0];

            for (i = 0; i < g_gt_tnum; i++)
            {
                if (tp_dev.sta & (1 << i))
                {
                    gt9xxx_rd_reg(GT9XXX_TPX_TBL[i], buf, 4);

                    if (lcddev.id == 0X5510 || lcddev.id == 0X9806 || lcddev.id == 0X7796)
                    {
                        if (tp_dev.touchtype & 0X01)
                        {
                            tp_dev.x[i] = lcddev.width - (((uint16_t)buf[3] << 8) + buf[2]);
                            tp_dev.y[i] = ((uint16_t)buf[1] << 8) + buf[0];
                        }
                        else
                        {
                            tp_dev.x[i] = ((uint16_t)buf[1] << 8) + buf[0];
                            tp_dev.y[i] = ((uint16_t)buf[3] << 8) + buf[2];
                        }
                    }
                    else
                    {
                        if (tp_dev.touchtype & 0X01)
                        {
                            tp_dev.x[i] = ((uint16_t)buf[1] << 8) + buf[0];
                            tp_dev.y[i] = ((uint16_t)buf[3] << 8) + buf[2];
                        }
                        else
                        {
                            tp_dev.x[i] = lcddev.width - 1U - (((uint16_t)buf[3] << 8) + buf[2]);
                            tp_dev.y[i] = ((uint16_t)buf[1] << 8) + buf[0];
                        }
                    }

                }
            }

            res = 1;

            if (tp_dev.x[0] > lcddev.width || tp_dev.y[0] > lcddev.height)
            {
                if ((mode & 0XF) > 1)
                {
                    tp_dev.x[0] = tp_dev.x[1];
                    tp_dev.y[0] = tp_dev.y[1];
                    t = 0;
                }
                else
                {
                    tp_dev.x[0] = tp_dev.x[g_gt_tnum - 1];
                    tp_dev.y[0] = tp_dev.y[g_gt_tnum - 1];
                    mode = 0X80;
                    tp_dev.sta = tempsta;
                }
            }
            else
            {
                t = 0;
            }
        }
    }

    if ((mode & 0X8F) == 0X80)
    {
        if (tp_dev.sta & TP_PRES_DOWN)
        {
            tp_dev.sta &= ~TP_PRES_DOWN;
        }
        else
        {
            tp_dev.x[0] = 0xffff;
            tp_dev.y[0] = 0xffff;
            tp_dev.sta &= 0XE000;
        }
    }

    if (t > 240)t = 10;

    return res;
}
