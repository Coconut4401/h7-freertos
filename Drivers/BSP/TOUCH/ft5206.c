/**
 * @file ft5206.c
 * @brief 驱动 FT5206 电容触摸控制器并读取多点触摸坐标。
 * @details 这是 ft5206 模块的实现文件（Drivers/BSP/TOUCH/ft5206.c）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#include "string.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/TOUCH/touch.h"
#include "./BSP/TOUCH/ctiic.h"
#include "./BSP/TOUCH/ft5206.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"

/**
 * @brief ft5206_wr_reg：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param reg 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param buf 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param len 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t ft5206_wr_reg(uint16_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t i;
    uint8_t ret = 0;
    ct_iic_start();
    ct_iic_send_byte(FT5206_CMD_WR);
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
 * @brief ft5206_rd_reg：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param reg 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param buf 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param len 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void ft5206_rd_reg(uint16_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t i;
    ct_iic_start();
    ct_iic_send_byte(FT5206_CMD_WR);
    ct_iic_wait_ack();
    ct_iic_send_byte(reg & 0XFF);
    ct_iic_wait_ack();
    ct_iic_start();
    ct_iic_send_byte(FT5206_CMD_RD);
    ct_iic_wait_ack();

    for (i = 0; i < len; i++)
    {
        buf[i] = ct_iic_read_byte(i == (len - 1) ? 0 : 1);
    }

    ct_iic_stop();
}

/**
 * @brief ft5206_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t ft5206_init(void)
{
    uint8_t temp[2];

    FT5206_RST_GPIO_CLK_ENABLE();
    FT5206_INT_GPIO_CLK_ENABLE();

    sys_gpio_set(FT5206_RST_GPIO_PORT, FT5206_RST_GPIO_PIN,
                 SYS_GPIO_MODE_OUT, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_MID, SYS_GPIO_PUPD_PU);

    sys_gpio_set(FT5206_INT_GPIO_PORT, FT5206_INT_GPIO_PIN,
                 SYS_GPIO_MODE_IN, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_MID, SYS_GPIO_PUPD_PU);

    ct_iic_init();
    FT5206_RST(0);
    delay_ms(20);
    FT5206_RST(1);
    delay_ms(50);

    if (ct_iic_check_device(FT5206_CMD_WR) != 0U)
    {
        printf("FT/CST I2C address 0x38: NACK\r\n");
        return 1;
    }

    temp[0] = 0;
    ft5206_wr_reg(FT5206_DEVIDE_MODE, temp, 1);
    ft5206_wr_reg(FT5206_ID_G_MODE, temp, 1);
    temp[0] = 22;
    ft5206_wr_reg(FT5206_ID_G_THGROUP, temp, 1);
    temp[0] = 12;
    ft5206_wr_reg(FT5206_ID_G_PERIODACTIVE, temp, 1);

    ft5206_rd_reg(FT5206_ID_G_LIB_VERSION, &temp[0], 2);

    if ((temp[0] == 0X30 && temp[1] == 0X03) || temp[1] == 0X01 || temp[1] == 0X02|| (temp[0] == 0x0 && temp[1] == 0X0))
    {
        printf("CTP ID:%x\r\n", ((uint16_t)temp[0] << 8) + temp[1]);
        return 0;
    }

    return 1;
}

const uint16_t FT5206_TPX_TBL[5] = {FT5206_TP1_REG, FT5206_TP2_REG, FT5206_TP3_REG, FT5206_TP4_REG, FT5206_TP5_REG};

/**
 * @brief ft5206_scan：扫描或采样当前输入与设备状态，整理本轮可用数据。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param mode 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t ft5206_scan(uint8_t mode)
{
    uint8_t sta = 0;
    uint8_t buf[4];
    uint8_t i = 0;
    uint8_t res = 0;
    uint16_t temp;
    static uint8_t t = 0;

    t++;

    if ((t % 10) == 0 || t < 10)
    {
        ft5206_rd_reg(FT5206_REG_NUM_FINGER, &sta, 1);

        if ((sta & 0XF) && ((sta & 0XF) < 6))
        {
            temp = 0XFFFF << (sta & 0XF);
            tp_dev.sta = (~temp) | TP_PRES_DOWN | TP_CATH_PRES;

            delay_ms(4);
            for (i = 0; i < 5; i++)
            {
                if (tp_dev.sta & (1 << i))
                {
                    ft5206_rd_reg(FT5206_TPX_TBL[i], buf, 4);

                    if (tp_dev.touchtype & 0X01)
                    {
                        tp_dev.y[i] = ((uint16_t)(buf[0] & 0X0F) << 8) + buf[1];
                        tp_dev.x[i] = ((uint16_t)(buf[2] & 0X0F) << 8) + buf[3];
                    }
                    else
                    {
                        tp_dev.x[i] = lcddev.width - (((uint16_t)(buf[0] & 0X0F) << 8) + buf[1]);
                        tp_dev.y[i] = ((uint16_t)(buf[2] & 0X0F) << 8) + buf[3];
                    }

                    if ((buf[0] & 0XF0) != 0X80)tp_dev.x[i] = tp_dev.y[i] = 0;

                }
            }

            res = 1;

            if (tp_dev.x[0] == 0 && tp_dev.y[0] == 0)sta = 0;

            t = 0;
        }
    }

    if ((sta & 0X1F) == 0)
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

    if (t > 240)
    {
        t = 10;
    }

    return res;
}
