/**
 * @file sys.c
 * @brief 封装时钟、GPIO、缓存和底层系统配置操作。
 * @details 这是 sys 模块的实现文件（Drivers/SYSTEM/sys/sys.c）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#include "./SYSTEM/sys/sys.h"

/**
 * @brief sys_nvic_set_vector_table：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param baseaddr 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param offset 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void sys_nvic_set_vector_table(uint32_t baseaddr, uint32_t offset)
{

    SCB->VTOR = baseaddr | (offset & (uint32_t)0xFFFFFE00);
}

/**
 * @brief sys_nvic_priority_group_config：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param group 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void sys_nvic_priority_group_config(uint8_t group)
{
    uint32_t temp, temp1;
    temp1 = (~group) & 0x07;
    temp1 <<= 8;
    temp = SCB->AIRCR;
    temp &= 0X0000F8FF;
    temp |= 0X05FA0000;
    temp |= temp1;
    SCB->AIRCR = temp;
}

/**
 * @brief sys_nvic_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param pprio 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param sprio 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param ch 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param group 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void sys_nvic_init(uint8_t pprio, uint8_t sprio, uint8_t ch, uint8_t group)
{
    uint32_t temp;
    sys_nvic_priority_group_config(group);
    temp = pprio << (4 - group);
    temp |= sprio & (0x0f >> group);
    temp &= 0xf;
    NVIC->ISER[ch / 32] |= 1 << (ch % 32);
    NVIC->IP[ch] |= temp << 4;
}

/**
 * @brief sys_nvic_ex_config：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param p_gpiox 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pinx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param tmode 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void sys_nvic_ex_config(GPIO_TypeDef *p_gpiox, uint16_t pinx, uint8_t tmode)
{
    uint8_t offset;
    uint32_t gpio_num = 0;
    uint32_t pinpos = 0, pos = 0, curpin = 0;

    gpio_num = ((uint32_t)p_gpiox - (uint32_t)GPIOA) / 0X400 ;
    RCC->APB4ENR |= 1 << 1;

    for (pinpos = 0; pinpos < 16; pinpos++)
    {
        pos = 1 << pinpos;
        curpin = pinx & pos;

        if (curpin == pos)
        {
            offset = (pinpos % 4) * 4;
            SYSCFG->EXTICR[pinpos / 4] &= ~(0x000F << offset);
            SYSCFG->EXTICR[pinpos / 4] |= gpio_num << offset;

            EXTI_D1->IMR1 |= 1 << pinpos;

            if (tmode & 0x01) EXTI->FTSR1 |= 1 << pinpos;
            if (tmode & 0x02) EXTI->RTSR1 |= 1 << pinpos;
        }
    }
}

/**
 * @brief sys_gpio_af_set：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param p_gpiox 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pinx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param afx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void sys_gpio_af_set(GPIO_TypeDef *p_gpiox, uint16_t pinx, uint8_t afx)
{
    uint32_t pinpos = 0, pos = 0, curpin = 0;;

    for (pinpos = 0; pinpos < 16; pinpos++)
    {
        pos = 1 << pinpos;
        curpin = pinx & pos;

        if (curpin == pos)
        {
            p_gpiox->AFR[pinpos >> 3] &= ~(0X0F << ((pinpos & 0X07) * 4));
            p_gpiox->AFR[pinpos >> 3] |= (uint32_t)afx << ((pinpos & 0X07) * 4);
        }
    }
}

/**
 * @brief sys_gpio_set：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param p_gpiox 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pinx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param mode 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param otype 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param ospeed 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pupd 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void sys_gpio_set(GPIO_TypeDef *p_gpiox, uint16_t pinx, uint32_t mode, uint32_t otype, uint32_t ospeed, uint32_t pupd)
{
    uint32_t pinpos = 0, pos = 0, curpin = 0;

    for (pinpos = 0; pinpos < 16; pinpos++)
    {
        pos = 1 << pinpos;
        curpin = pinx & pos;

        if (curpin == pos)
        {
            p_gpiox->MODER &= ~(3 << (pinpos * 2));
            p_gpiox->MODER |= mode << (pinpos * 2);

            if ((mode == 0X01) || (mode == 0X02))
            {
                p_gpiox->OSPEEDR &= ~(3 << (pinpos * 2));
                p_gpiox->OSPEEDR |= (ospeed << (pinpos * 2));
                p_gpiox->OTYPER &= ~(1 << pinpos) ;
                p_gpiox->OTYPER |= otype << pinpos;
            }

            p_gpiox->PUPDR &= ~(3 << (pinpos * 2));
            p_gpiox->PUPDR |= pupd << (pinpos * 2);
        }
    }
}

/**
 * @brief sys_gpio_pin_set：将指定 GPIO 引脚设置为高电平或低电平。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param p_gpiox 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pinx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param status 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void sys_gpio_pin_set(GPIO_TypeDef *p_gpiox, uint16_t pinx, uint8_t status)
{
    if (status & 0X01)
    {
        p_gpiox->BSRR |= pinx;
    }
    else
    {
        p_gpiox->BSRR |= (uint32_t)pinx << 16;
    }
}

/**
 * @brief sys_gpio_pin_get：读取指定 GPIO 输入数据寄存器并返回引脚逻辑电平。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param p_gpiox 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pinx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t sys_gpio_pin_get(GPIO_TypeDef *p_gpiox, uint16_t pinx)
{
    if (p_gpiox->IDR & pinx)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

/**
 * @brief sys_wfi_set：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void sys_wfi_set(void)
{
    __ASM volatile("wfi");
}

/**
 * @brief sys_intx_disable：停止或禁用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void sys_intx_disable(void)
{
    __ASM volatile("cpsid i");
}

/**
 * @brief sys_intx_enable：启动或启用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void sys_intx_enable(void)
{
    __ASM volatile("cpsie i");
}

/**
 * @brief sys_msr_msp：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param addr 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void sys_msr_msp(uint32_t addr)
{
    __set_MSP(addr);
}

/**
 * @brief sys_standby：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void sys_standby(void)
{
    PWR->WKUPEPR &= ~(1 << 0);
    PWR->WKUPEPR |= 1 << 0;
    PWR->WKUPEPR &= ~(1 << 8);
    PWR->WKUPEPR &= ~(3 << 16);
    PWR->WKUPEPR |= 2 << 16;
    PWR->WKUPCR |= 0X3F << 0;
    PWR->CPUCR |= 7 << 0;
    SCB->SCR |= 1 << 2;
    sys_wfi_set();
}

/**
 * @brief sys_soft_reset：清除已有状态或复位目标设备，使其回到约定的初始状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void sys_soft_reset(void)
{
    SCB->AIRCR = 0X05FA0000 | (uint32_t)0x04;
}

/**
 * @brief sys_cache_enable：启动或启用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void sys_cache_enable(void)
{
    SCB_EnableICache();
    SCB_EnableDCache();
    SCB->CACR |= 1 << 2;
}

/**
 * @brief sys_clock_set：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param plln 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pllm 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pllp 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pllq 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t sys_clock_set(uint32_t plln, uint32_t pllm, uint32_t pllp, uint32_t pllq)
{
    uint32_t retry = 0;
    uint8_t retval = 0;
    uint8_t swsval = 0;

    PWR->CR3 &= ~(1 << 2);
    PWR->D3CR |= 3 << 14;

    while ((PWR->D3CR & (1 << 13)) == 0);

    RCC->CR |= 1 << 16;

    while (((RCC->CR & (1 << 17)) == 0) && (retry < 0X7FFF))
    {
        retry++;
    }

    if (retry == 0X7FFF)
    {
        retval = 1;
    }
    else
    {
        RCC->PLLCKSELR |= 2 << 0;
        RCC->PLLCKSELR |= pllm << 4;
        RCC->PLL1DIVR |= (plln - 1) << 0;
        RCC->PLL1DIVR |= (pllp - 1) << 9;
        RCC->PLL1DIVR |= (pllq - 1) << 16;
        RCC->PLL1DIVR |= 1 << 24;
        RCC->PLLCFGR |= 2 << 2;
        RCC->PLLCFGR |= 0 << 1;
        RCC->PLLCFGR |= 3 << 16;
        RCC->CR |= 1 << 24;
        retry = 0;

        while ((RCC->CR & (1 << 25)) == 0)
        {
            retry++;

            if (retry > 0X1FFFFF)
            {
                retval = 2;
                break;
            }
        }

        RCC->PLLCKSELR |= 25 << 12;
        RCC->PLL2DIVR |= (440 - 1) << 0;
        RCC->PLL2DIVR |= (2 - 1) << 9;
        RCC->PLL2DIVR |= (2 - 1) << 24;
        RCC->PLLCFGR |= 0 << 6;
        RCC->PLLCFGR |= 0 << 5;
        RCC->PLLCFGR |= 1 << 19;
        RCC->PLLCFGR |= 1 << 21;
        RCC->D1CCIPR &= ~(3 << 0);
        RCC->D1CCIPR |= 2 << 0;
        RCC->CR |= 1 << 26;
        retry = 0;

        while ((RCC->CR & (1 << 27)) == 0)
        {
            retry++;

            if (retry > 0X1FFFFF)
            {
                retval = 3;
                break;
            }
        }

        RCC->D1CFGR |= 8 << 0;
        RCC->D1CFGR |= 0 << 8;
        RCC->CFGR |= 3 << 0;
        retry = 0;

        while (swsval != 3)
        {
            swsval = (RCC->CFGR & (7 << 3)) >> 3;
            retry++;

            if (retry > 0X1FFFFF)
            {
                retval = 4;
                break;
            }
        }

        FLASH->ACR |= 2 << 0;
        FLASH->ACR |= 2 << 4;
        RCC->D1CFGR |= 4 << 4;
        RCC->D2CFGR |= 4 << 4;
        RCC->D2CFGR |= 4 << 8;
        RCC->D3CFGR |= 4 << 4;

        RCC->CR |= 1 << 7;
        RCC->APB4ENR |= 1 << 1;
        SYSCFG->CCCSR |= 1 << 0;
    }

    return retval;
}

/**
 * @brief sys_stm32_clock_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param plln 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pllm 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pllp 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pllq 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void sys_stm32_clock_init(uint32_t plln, uint32_t pllm, uint32_t pllp, uint32_t pllq)
{
    RCC->CR = 0x00000001;
    RCC->CFGR = 0x00000000;
    RCC->D1CFGR = 0x00000000;
    RCC->D2CFGR = 0x00000000;
    RCC->D3CFGR = 0x00000000;
    RCC->PLLCKSELR = 0x00000000;
    RCC->PLLCFGR = 0x00000000;
    RCC->CIER = 0x00000000;

    GPV->AXI_TARG7_FN_MOD = 0x00000001;

    sys_clock_set(plln, pllm, pllp, pllq);
    sys_cache_enable();

#ifdef  VECT_TAB_RAM
    sys_nvic_set_vector_table(D1_AXISRAM_BASE, 0x0);
#else
    sys_nvic_set_vector_table(FLASH_BANK1_BASE, 0x0);
#endif
}
