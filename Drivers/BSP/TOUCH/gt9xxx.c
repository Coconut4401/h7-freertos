

#include "string.h"
#include "./BSP/LCD/lcd.h"
#include "./BSP/TOUCH/touch.h"
#include "./BSP/TOUCH/ctiic.h"
#include "./BSP/TOUCH/gt9xxx.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"



/* 注意: 除了GT9271支持10点触摸之外, 其他触摸芯片只支持 5点触摸 */
uint8_t g_gt_tnum = 5;      /* 默认支持的触摸屏点数(5点触摸) */
static uint8_t g_gt_cmd_wr = GT9XXX_ADDR_14_WR;
static uint8_t g_gt_cmd_rd = GT9XXX_ADDR_14_RD;

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

uint8_t gt9xxx_get_i2c_address(void)
{
    return g_gt_cmd_wr >> 1;
}

/**
 * @brief       向gt9xxx写入一次数据
 * @param       reg : 起始寄存器地址
 * @param       buf : 数据缓缓存区
 * @param       len : 写数据长度
 * @retval      0, 成功; 1, 失败;
 */
uint8_t gt9xxx_wr_reg(uint16_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t i;
    uint8_t ret = 0;
    ct_iic_start();
    ct_iic_send_byte(g_gt_cmd_wr);      /* 发送写命令 */
    ct_iic_wait_ack();
    ct_iic_send_byte(reg >> 8);         /* 发送高8位地址 */
    ct_iic_wait_ack();
    ct_iic_send_byte(reg & 0XFF);       /* 发送低8位地址 */
    ct_iic_wait_ack();

    for (i = 0; i < len; i++)
    {
        ct_iic_send_byte(buf[i]);       /* 发数据 */
        ret = ct_iic_wait_ack();

        if (ret)break;
    }

    ct_iic_stop();  /* 产生一个停止条件 */
    return ret;
}

/**
 * @brief       从gt9xxx读出一次数据
 * @param       reg : 起始寄存器地址
 * @param       buf : 数据缓缓存区
 * @param       len : 读数据长度
 * @retval      无
 */
void gt9xxx_rd_reg(uint16_t reg, uint8_t *buf, uint8_t len)
{
    uint8_t i;
    ct_iic_start();
    ct_iic_send_byte(g_gt_cmd_wr);      /* 发送写命令 */
    ct_iic_wait_ack();
    ct_iic_send_byte(reg >> 8);         /* 发送高8位地址 */
    ct_iic_wait_ack();
    ct_iic_send_byte(reg & 0XFF);       /* 发送低8位地址 */
    ct_iic_wait_ack();
    ct_iic_start();
    ct_iic_send_byte(g_gt_cmd_rd);      /* 发送读命令 */
    ct_iic_wait_ack();

    for (i = 0; i < len; i++)
    {
        buf[i] = ct_iic_read_byte(i == (len - 1) ? 0 : 1);  /* 发数据 */
    }

    ct_iic_stop();  /* 产生一个停止条件 */
}

/**
 * @brief       初始化gt9xxx触摸屏
 * @param       无
 * @retval      0, 初始化成功; 1, 初始化失败;
 */
uint8_t gt9xxx_init(void)
{
    uint8_t temp[5];

    GT9XXX_RST_GPIO_CLK_ENABLE();   /* RST引脚时钟使能 */
    GT9XXX_INT_GPIO_CLK_ENABLE();   /* INT引脚时钟使能 */

    /* RST引脚模式设置, 推挽输出, 上拉 */
    sys_gpio_set(GT9XXX_RST_GPIO_PORT, GT9XXX_RST_GPIO_PIN,
                 SYS_GPIO_MODE_OUT, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_MID, SYS_GPIO_PUPD_PU);

    ct_iic_init();      /* 初始化电容屏的I2C总线 */

    /* INT high selects 7-bit address 0x14. Retry with INT low for 0x5D. */
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

    gt9xxx_rd_reg(GT9XXX_PID_REG, temp, 4); /* 读取产品ID */
    temp[4] = 0;
    printf("GT9xxx PID raw: %02X %02X %02X %02X, text: %s\r\n",
           temp[0], temp[1], temp[2], temp[3], temp);

    /* Goodix variants share the same register protocol but use different IDs. */
    if (!gt9xxx_pid_valid(temp))
    {
        printf("GT9xxx product ID is not valid ASCII\r\n");
        return 1;
    }
    printf("CTP ID:%s\r\n", temp);          /* 打印ID */
    
    if (strcmp((char *)temp, "9271") == 0)  /* ID==9271, 支持10点触摸 */
    {
         g_gt_tnum = 10;    /* 支持10点触摸屏 */
    }
    
    temp[0] = 0X02;
    gt9xxx_wr_reg(GT9XXX_CTRL_REG, temp, 1);    /* 软复位GT9XXX */
    
    delay_ms(10);
    
    temp[0] = 0X00;
    gt9xxx_wr_reg(GT9XXX_CTRL_REG, temp, 1);    /* 结束复位, 进入读坐标状态 */

    return 0;
}

/* GT9XXX 10个触摸点(最多) 对应的寄存器表 */
const uint16_t GT9XXX_TPX_TBL[10] =
{
    GT9XXX_TP1_REG, GT9XXX_TP2_REG, GT9XXX_TP3_REG, GT9XXX_TP4_REG, GT9XXX_TP5_REG,
    GT9XXX_TP6_REG, GT9XXX_TP7_REG, GT9XXX_TP8_REG, GT9XXX_TP9_REG, GT9XXX_TP10_REG,
};

/**
 * @brief       扫描触摸屏(采用查询方式)
 * @param       mode : 电容屏未用到次参数, 为了兼容电阻屏
 * @retval      当前触屏状态
 *   @arg       0, 触屏无触摸; 
 *   @arg       1, 触屏有触摸;
 */
uint8_t gt9xxx_scan(uint8_t mode)
{
    uint8_t buf[4];
    uint8_t i = 0;
    uint8_t res = 0;
    uint16_t temp;
    uint16_t tempsta;
    static uint8_t t = 0;   /* 控制查询间隔,从而降低CPU占用率 */
    t++;

    if ((t % 10) == 0 || t < 10)    /* 空闲时,每进入10次CTP_Scan函数才检测1次,从而节省CPU使用率 */
    {
        gt9xxx_rd_reg(GT9XXX_GSTID_REG, &mode, 1);  /* 读取触摸点的状态 */

        if ((mode & 0X80) && ((mode & 0XF) <= g_gt_tnum))
        {
            i = 0;
            gt9xxx_wr_reg(GT9XXX_GSTID_REG, &i, 1); /* 清标志 */
        }

        if ((mode & 0XF) && ((mode & 0XF) <= g_gt_tnum))
        {
            temp = 0XFFFF << (mode & 0XF);  /* 将点的个数转换为1的位数,匹配tp_dev.sta定义 */
            tempsta = tp_dev.sta;           /* 保存当前的tp_dev.sta值 */
            tp_dev.sta = (~temp) | TP_PRES_DOWN | TP_CATH_PRES;
            tp_dev.x[g_gt_tnum - 1] = tp_dev.x[0];  /* 保存触点0的数据,保存在最后一个上 */
            tp_dev.y[g_gt_tnum - 1] = tp_dev.y[0];

            for (i = 0; i < g_gt_tnum; i++)
            {
                if (tp_dev.sta & (1 << i))  /* 触摸有效? */
                {
                    gt9xxx_rd_reg(GT9XXX_TPX_TBL[i], buf, 4);   /* 读取XY坐标值 */

                    if (lcddev.id == 0X5510 || lcddev.id == 0X9806 || lcddev.id == 0X7796)     /* 4.3寸800*480 和 3.5寸480*320 MCU屏 */
                    {
                        if (tp_dev.touchtype & 0X01)    /* 横屏 */
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

                    //printf("x[%d]:%d,y[%d]:%d\r\n", i, tp_dev.x[i], i, tp_dev.y[i]);
                }
            }

            res = 1;

            if (tp_dev.x[0] > lcddev.width || tp_dev.y[0] > lcddev.height)  /* 非法数据(坐标超出了) */
            {
                if ((mode & 0XF) > 1)   /* 有其他点有数据,则复第二个触点的数据到第一个触点. */
                {
                    tp_dev.x[0] = tp_dev.x[1];
                    tp_dev.y[0] = tp_dev.y[1];
                    t = 0;  /* 触发一次,则会最少连续监测10次,从而提高命中率 */
                }
                else        /* 非法数据,则忽略此次数据(还原原来的) */
                {
                    tp_dev.x[0] = tp_dev.x[g_gt_tnum - 1];
                    tp_dev.y[0] = tp_dev.y[g_gt_tnum - 1];
                    mode = 0X80;
                    tp_dev.sta = tempsta;   /* 恢复tp_dev.sta */
                }
            }
            else 
            {
                t = 0;      /* 触发一次,则会最少连续监测10次,从而提高命中率 */
            }
        }
    }

    if ((mode & 0X8F) == 0X80)  /* 无触摸点按下 */
    {
        if (tp_dev.sta & TP_PRES_DOWN)      /* 之前是被按下的 */
        {
            tp_dev.sta &= ~TP_PRES_DOWN;    /* 标记按键松开 */
        }
        else    /* 之前就没有被按下 */
        {
            tp_dev.x[0] = 0xffff;
            tp_dev.y[0] = 0xffff;
            tp_dev.sta &= 0XE000;           /* 清除点有效标记 */
        }
    }

    if (t > 240)t = 10; /* 重新从10开始计数 */

    return res;
}




























