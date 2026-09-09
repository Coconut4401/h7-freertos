/**
 * @file sdram.c
 * @brief 初始化 FMC SDRAM 时序并发送器件上电配置命令。
 * @details 这是 sdram 模块的实现文件（Drivers/BSP/SDRAM/sdram.c）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#include "./SYSTEM/delay/delay.h"
#include "./BSP/SDRAM/sdram.h"

/**
 * @brief sdram_send_cmd：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param bankx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param cmd 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param refresh 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param regval 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t sdram_send_cmd(uint8_t bankx, uint8_t cmd, uint8_t refresh, uint16_t regval)
{
    uint32_t retry = 0;
    uint32_t tempreg = 0;

    tempreg |= cmd << 0;
    tempreg |= 1 << (4 - bankx);
    tempreg |= refresh << 5;
    tempreg |= regval << 9;
    FMC_Bank5_6_R->SDCMR = tempreg;

    while ((FMC_Bank5_6_R->SDSR & (1 << 5)))
    {
        retry++;

        if (retry > 0X1FFFFF)return 1;
    }

    return 0;
}

/**
 * @brief sdram_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void sdram_init(void)
{
    uint32_t sdctrlreg = 0, sdtimereg = 0;
    uint16_t mregval = 0;

    RCC->AHB4ENR |= 0X1F << 2;
    RCC->AHB3ENR |= 1 << 12;

    sys_gpio_set(GPIOC, SYS_GPIO_PIN0 | SYS_GPIO_PIN2 | SYS_GPIO_PIN3,
                 SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);

    sys_gpio_set(GPIOD, 3 << 0 | 7 << 8 | 3 << 14,
                 SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);

    sys_gpio_set(GPIOE, 3 << 0 | 0X1FF << 7,
                 SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);

    sys_gpio_set(GPIOF, 0X3F << 0 | 0X1F << 11,
                 SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);

    sys_gpio_set(GPIOG, 7 << 0 | 3 << 4 | SYS_GPIO_PIN8 | SYS_GPIO_PIN15,
                 SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);

    sys_gpio_af_set(GPIOC, SYS_GPIO_PIN0 | SYS_GPIO_PIN2 | SYS_GPIO_PIN3, 12);
    sys_gpio_af_set(GPIOD, 3 << 0 | 7 << 8 | 3 << 14, 12);
    sys_gpio_af_set(GPIOE, 3 << 0 | 0X1FF << 7, 12);
    sys_gpio_af_set(GPIOF, 0X3F << 0 | 0X1F << 11, 12);
    sys_gpio_af_set(GPIOG, 7 << 0 | 3 << 4 | SYS_GPIO_PIN8 | SYS_GPIO_PIN15, 12);

    sdctrlreg |= 1 << 0;
    sdctrlreg |= 2 << 2;
    sdctrlreg |= 1 << 4;
    sdctrlreg |= 1 << 6;
    sdctrlreg |= 2 << 7;
    sdctrlreg |= 0 << 9;
    sdctrlreg |= 2 << 10;
    sdctrlreg |= 1 << 12;
    sdctrlreg |= 0 << 13;
    FMC_Bank5_6_R->SDCR[0] = sdctrlreg;

    sdtimereg |= 1 << 0;
    sdtimereg |= 7 << 4;
    sdtimereg |= 6 << 8;
    sdtimereg |= 6 << 12;
    sdtimereg |= 1 << 16;
    sdtimereg |= 1 << 20;
    sdtimereg |= 1 << 24;
    FMC_Bank5_6_R->SDTR[0] = sdtimereg;
    FMC_Bank1_R->BTCR[0] |= (uint32_t)1 << 31;

    sdram_send_cmd(0, 1, 0, 0);
    delay_us(500);
    sdram_send_cmd(0, 2, 0, 0);
    sdram_send_cmd(0, 3, 8, 0);
    mregval |= 1 << 0;
    mregval |= 0 << 3;
    mregval |= 2 << 4;
    mregval |= 0 << 7;
    mregval |= 1 << 9;
    sdram_send_cmd(0, 4, 0, mregval);

    FMC_Bank5_6_R->SDRTR = 839 << 1;
}

/**
 * @brief fmc_sdram_write_buffer：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param pbuf 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param writeaddr 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param n 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void fmc_sdram_write_buffer(uint8_t *pbuf, uint32_t writeaddr, uint32_t n)
{
    for (; n != 0; n--)
    {
        *(volatile uint8_t *)(BANK5_SDRAM_ADDR + writeaddr) = *pbuf;
        writeaddr++;
        pbuf++;
    }
}

/**
 * @brief fmc_sdram_read_buffer：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param pbuf 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param readaddr 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param n 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void fmc_sdram_read_buffer(uint8_t *pbuf, uint32_t readaddr, uint32_t n)
{
    for (; n != 0; n--)
    {
        *pbuf++ = *(volatile uint8_t *)(BANK5_SDRAM_ADDR + readaddr);
        readaddr++;
    }
}
