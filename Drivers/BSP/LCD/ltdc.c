/**
 * @file ltdc.c
 * @brief 配置 STM32 LTDC、图层和帧缓冲，并控制显示时序与混合。
 * @details 这是 ltdc 模块的实现文件（Drivers/BSP/LCD/ltdc.c）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#include "./BSP/LCD/lcd.h"
#include "./BSP/LCD/ltdc.h"
#include "./SYSTEM/delay/delay.h"

#if !(__ARMCC_VERSION >= 6010050)

#if LTDC_PIXFORMAT == LTDC_PIXFORMAT_ARGB8888 || LTDC_PIXFORMAT == LTDC_PIXFORMAT_RGB888
    uint32_t ltdc_lcd_framebuf[1280][800] __attribute__((at(LTDC_FRAME_BUF_ADDR)));
#else
    uint16_t ltdc_lcd_framebuf[1280][800] __attribute__((at(LTDC_FRAME_BUF_ADDR)));

#endif

#else

#if LTDC_PIXFORMAT == LTDC_PIXFORMAT_ARGB8888 || LTDC_PIXFORMAT == LTDC_PIXFORMAT_RGB888
    uint32_t ltdc_lcd_framebuf[1280][800] __attribute__((section(".bss.ARM.__at_0XC0000000")));
#else
    uint16_t ltdc_lcd_framebuf[1280][800] __attribute__((section(".bss.ARM.__at_0XC0000000")));
#endif

#endif

uint32_t *g_ltdc_framebuf[2];
_ltdc_dev lcdltdc;

/**
 * @brief ltdc_switch：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param sw 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void ltdc_switch(uint8_t sw)
{
    if (sw)
    {
        LTDC->GCR |= 1 << 0;
    }
    else
    {
        LTDC->GCR &= ~(1 << 0);
    }
}

/**
 * @brief ltdc_layer_switch：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param layerx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param sw 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void ltdc_layer_switch(uint8_t layerx, uint8_t sw)
{
    if (sw)
    {
        if (layerx == 0)LTDC_Layer1->CR |= 1 << 0;
        else LTDC_Layer2->CR |= 1 << 0;
    }
    else
    {
        if (layerx == 0)LTDC_Layer1->CR &= ~(1 << 0);
        else LTDC_Layer2->CR &= ~(1 << 0);
    }

    LTDC->SRCR |= 1 << 0;
}

/**
 * @brief ltdc_select_layer：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param layerx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void ltdc_select_layer(uint8_t layerx)
{
    lcdltdc.activelayer = layerx;
}

/**
 * @brief ltdc_display_dir：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param dir 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void ltdc_display_dir(uint8_t dir)
{
    lcdltdc.dir = dir;

    if (dir == 0)
    {
        lcdltdc.width = lcdltdc.pheight;
        lcdltdc.height = lcdltdc.pwidth;
    }
    else if (dir == 1)
    {
        lcdltdc.width = lcdltdc.pwidth;
        lcdltdc.height = lcdltdc.pheight;
    }
}

/**
 * @brief ltdc_draw_point：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void ltdc_draw_point(uint16_t x, uint16_t y, uint32_t color)
{
#if LTDC_PIXFORMAT == LTDC_PIXFORMAT_ARGB8888 || LTDC_PIXFORMAT == LTDC_PIXFORMAT_RGB888

    if (lcdltdc.dir)
    {
        *(uint32_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * y + x)) = color;
    }
    else
    {
        *(uint32_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - 1) + y)) = color;
    }

#else

    if (lcdltdc.dir)
    {
        *(uint16_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * y + x)) = color;
    }
    else
    {
        *(uint16_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - 1) + y)) = color;
    }

#endif
}

/**
 * @brief ltdc_read_point：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint32_t ltdc_read_point(uint16_t x, uint16_t y)
{
#if LTDC_PIXFORMAT == LTDC_PIXFORMAT_ARGB8888 || LTDC_PIXFORMAT == LTDC_PIXFORMAT_RGB888

    if (lcdltdc.dir)
    {
        return *(uint32_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * y + x));
    }
    else
    {
        return *(uint32_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - 1) + y));
    }

#else

    if (lcdltdc.dir)
    {
        return *(uint16_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * y + x));
    }
    else
    {
        return *(uint16_t *)((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * (lcdltdc.pheight - x - 1) + y));
    }

#endif
}

/**
 * @brief ltdc_fill：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param sx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param sy 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param ex 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param ey 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void ltdc_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint32_t color)
{
    uint32_t psx, psy, pex, pey;
    uint32_t timeout = 0;
    uint16_t offline;
    uint32_t addr;

    if (lcdltdc.dir)
    {
        psx = sx;
        psy = sy;
        pex = ex;
        pey = ey;
    }
    else
    {
        if(ex >= lcdltdc.pheight)ex = lcdltdc.pheight - 1;
        if(sx >= lcdltdc.pheight)sx = lcdltdc.pheight - 1;

        psx = sy;
        psy = lcdltdc.pheight - ex - 1;
        pex = ey;
        pey = lcdltdc.pheight - sx - 1;
    }

    offline = lcdltdc.pwidth - (pex - psx + 1);
    addr = ((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * psy + psx));
    RCC->AHB3ENR |= 1 << 4;
    DMA2D->CR &= ~(1 << 0);
    DMA2D->CR = 3 << 16;
    DMA2D->OPFCCR = LTDC_PIXFORMAT;
    DMA2D->OOR = offline;
    DMA2D->OMAR = addr;
    DMA2D->NLR = (pey - psy + 1) | ((pex - psx + 1) << 16);
    DMA2D->OCOLR = color;
    DMA2D->CR |= 1 << 0;

    while ((DMA2D->ISR & (1 << 1)) == 0)
    {
        timeout++;

        if (timeout > 0X1FFFFF)break;
    }

    DMA2D->IFCR |= 1 << 1;
}

/**
 * @brief ltdc_color_fill：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param sx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param sy 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param ex 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param ey 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void ltdc_color_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color)
{
    uint32_t psx, psy, pex, pey;
    uint32_t timeout = 0;
    uint16_t offline;
    uint32_t addr;

    if (lcdltdc.dir)
    {
        psx = sx;
        psy = sy;
        pex = ex;
        pey = ey;
    }
    else
    {
        psx = sy;
        psy = lcdltdc.pheight - ex - 1;
        pex = ey;
        pey = lcdltdc.pheight - sx - 1;
    }

    offline = lcdltdc.pwidth - (pex - psx + 1);
    addr = ((uint32_t)g_ltdc_framebuf[lcdltdc.activelayer] + lcdltdc.pixsize * (lcdltdc.pwidth * psy + psx));
    RCC->AHB3ENR |= 1 << 4;
    DMA2D->CR &= ~(1 << 0);
    DMA2D->CR = 0 << 16;
    DMA2D->FGPFCCR = LTDC_PIXFORMAT;
    DMA2D->FGOR = 0;
    DMA2D->OOR = offline;
    DMA2D->FGMAR = (uint32_t)color;
    DMA2D->OMAR = addr;
    DMA2D->NLR = (pey - psy + 1) | ((pex - psx + 1) << 16);
    DMA2D->CR |= 1 << 0;

    while ((DMA2D->ISR & (1 << 1)) == 0)
    {
        timeout++;

        if (timeout > 0X1FFFFF)break;
    }

    DMA2D->IFCR |= 1 << 1;
}

/**
 * @brief ltdc_clear：清除已有状态或复位目标设备，使其回到约定的初始状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void ltdc_clear(uint32_t color)
{
    ltdc_fill(0, 0, lcdltdc.width - 1, lcdltdc.height - 1, color);
}

/**
 * @brief ltdc_clk_set：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param pll3n 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pll3m 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pll3r 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t ltdc_clk_set(uint32_t pll3n, uint32_t pll3m, uint32_t pll3r)
{
    uint16_t retry = 0;
    uint8_t status = 0;

    RCC->CR &= ~(1 << 28);

    while (((RCC->CR & (1 << 29))) && (retry < 0X1FFF))retry++;

    if (retry == 0X1FFF)status = 1;
    else
    {
        RCC->PLLCKSELR &= ~(0X3F << 20);
        RCC->PLLCKSELR |= pll3m << 20;
        RCC->PLL3DIVR &= ~(0X1FF << 0);
        RCC->PLL3DIVR |= (pll3n - 1) << 0;
        RCC->PLL3DIVR &= ~(0X7F << 24);
        RCC->PLL3DIVR |= (pll3r - 1) << 24;

        RCC->PLLCFGR &= ~(0X0F << 8);
        RCC->PLLCFGR |= 0 << 10;
        RCC->PLLCFGR |= 0 << 9;
        RCC->PLLCFGR |= 1 << 24;
        RCC->CR |= 1 << 28;

        while (((RCC->CR & (1 << 29)) == 0) && (retry < 0X1FFF))retry++;

        if (retry == 0X1FFF)status = 2;
    }

    return status;
}

/**
 * @brief ltdc_layer_window_config：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param layerx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param sx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param sy 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param width 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param height 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void ltdc_layer_window_config(uint8_t layerx, uint16_t sx, uint16_t sy, uint16_t width, uint16_t height)
{
    uint32_t temp;
    uint8_t pixformat = 0;

    if (layerx == 0)
    {
        temp = (sx + width + ((LTDC->BPCR & 0X0FFF0000) >> 16)) << 16;
        LTDC_Layer1->WHPCR = (sx + ((LTDC->BPCR & 0X0FFF0000) >> 16) + 1) | temp;
        temp = (sy + height + (LTDC->BPCR & 0X7FF)) << 16;
        LTDC_Layer1->WVPCR = (sy + (LTDC->BPCR & 0X7FF) + 1) | temp;
        pixformat = LTDC_Layer1->PFCR & 0X07;

        if (pixformat == 0)temp = 4;
        else if (pixformat == 1)temp = 3;
        else if (pixformat == 5 || pixformat == 6)temp = 1;
        else temp = 2;

        LTDC_Layer1->CFBLR = (width * temp << 16) | (width * temp + 3);
        LTDC_Layer1->CFBLNR = height;
    }
    else
    {
        temp = (sx + width + ((LTDC->BPCR & 0X0FFF0000) >> 16)) << 16;
        LTDC_Layer2->WHPCR = (sx + ((LTDC->BPCR & 0X0FFF0000) >> 16) + 1) | temp;
        temp = (sy + height + (LTDC->BPCR & 0X7FF)) << 16;
        LTDC_Layer2->WVPCR = (sy + (LTDC->BPCR & 0X7FF) + 1) | temp;
        pixformat = LTDC_Layer2->PFCR & 0X07;

        if (pixformat == 0)temp = 4;
        else if (pixformat == 1)temp = 3;
        else if (pixformat == 5 || pixformat == 6)temp = 1;
        else temp = 2;

        LTDC_Layer2->CFBLR = (width * temp << 16) | (width * temp + 3);
        LTDC_Layer2->CFBLNR = height;
    }

    ltdc_layer_switch(layerx, 1);
}

/**
 * @brief ltdc_layer_parameter_config：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param layerx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param bufaddr 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pixformat 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param alpha 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param alpha0 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param bfac1 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param bfac2 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param bkcolor 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void ltdc_layer_parameter_config(uint8_t layerx, uint32_t bufaddr, uint8_t pixformat, uint8_t alpha, uint8_t alpha0, uint8_t bfac1, uint8_t bfac2, uint32_t bkcolor)
{
    if (layerx == 0)
    {
        LTDC_Layer1->CFBAR = bufaddr;
        LTDC_Layer1->PFCR = pixformat;
        LTDC_Layer1->CACR = alpha;
        LTDC_Layer1->DCCR = ((uint32_t)alpha0 << 24) | bkcolor;
        LTDC_Layer1->BFCR = ((uint32_t)bfac1 << 8) | bfac2;
    }
    else
    {
        LTDC_Layer2->CFBAR = bufaddr;
        LTDC_Layer2->PFCR = pixformat;
        LTDC_Layer2->CACR = alpha;
        LTDC_Layer2->DCCR = ((uint32_t)alpha0 << 24) | bkcolor;
        LTDC_Layer2->BFCR = ((uint32_t)bfac1 << 8) | bfac2;
    }
}

/**
 * @brief ltdc_panelid_read：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint16_t ltdc_panelid_read(void)
{

    return 0X7084;
}

/**
 * @brief ltdc_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void ltdc_init(void)
{
    uint32_t tempreg = 0;
    uint16_t lcdid = 0;

    lcdid = ltdc_panelid_read();

    LTDC_BL_GPIO_CLK_ENABLE();
    LTDC_DE_GPIO_CLK_ENABLE();
    LTDC_VSYNC_GPIO_CLK_ENABLE();
    LTDC_HSYNC_GPIO_CLK_ENABLE();
    LTDC_CLK_GPIO_CLK_ENABLE();

    sys_gpio_set(LTDC_BL_GPIO_PORT, LTDC_BL_GPIO_PIN,
                 SYS_GPIO_MODE_OUT, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);

    sys_gpio_set(LTDC_DE_GPIO_PORT, LTDC_DE_GPIO_PIN,
                 SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);

    sys_gpio_set(LTDC_VSYNC_GPIO_PORT, LTDC_VSYNC_GPIO_PIN,
                 SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);

    sys_gpio_set(LTDC_HSYNC_GPIO_PORT, LTDC_HSYNC_GPIO_PIN,
                 SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);

    sys_gpio_set(LTDC_CLK_GPIO_PORT, LTDC_CLK_GPIO_PIN,
                 SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);

    sys_gpio_af_set(LTDC_DE_GPIO_PORT, LTDC_DE_GPIO_PIN, 14);
    sys_gpio_af_set(LTDC_VSYNC_GPIO_PORT, LTDC_VSYNC_GPIO_PIN, 14);
    sys_gpio_af_set(LTDC_HSYNC_GPIO_PORT, LTDC_HSYNC_GPIO_PIN, 14);
    sys_gpio_af_set(LTDC_CLK_GPIO_PORT, LTDC_CLK_GPIO_PIN, 14);

    RCC->APB3ENR |= 1 << 3;
    RCC->AHB4ENR |= 0X7 << 6;

    sys_gpio_set(GPIOG, SYS_GPIO_PIN6 | SYS_GPIO_PIN11,
                 SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);

    sys_gpio_set(GPIOH, 0X7F << 9,
                 SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);

    sys_gpio_set(GPIOI, 7 << 0 | 0XF << 4,
                 SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_HIGH, SYS_GPIO_PUPD_PU);

    sys_gpio_af_set(GPIOG, SYS_GPIO_PIN6 | SYS_GPIO_PIN11, 14);
    sys_gpio_af_set(GPIOH, 0X7F << 9, 14);
    sys_gpio_af_set(GPIOI, 7 << 0 | 0XF << 4, 14);

    if (lcdid == 0X4342)
    {
        lcdltdc.pwidth = 480;
        lcdltdc.pheight = 272;
        lcdltdc.hsw = 1;
        lcdltdc.vsw = 1;
        lcdltdc.hbp = 40;
        lcdltdc.vbp = 8;
        lcdltdc.hfp = 5;
        lcdltdc.vfp = 8;
        ltdc_clk_set(300, 25, 33);
    }
    else if (lcdid == 0X7084)
    {
        lcdltdc.pwidth = 800;
        lcdltdc.pheight = 480;
        lcdltdc.hsw = 1;
        lcdltdc.vsw = 1;
        lcdltdc.hbp = 46;
        lcdltdc.vbp = 23;
        lcdltdc.hfp = 210;
        lcdltdc.vfp = 22;
        ltdc_clk_set(300, 25, 9);
    }
    else if (lcdid == 0X7016)
    {
        lcdltdc.pwidth = 1024;
        lcdltdc.pheight = 600;
        lcdltdc.hsw = 20;
        lcdltdc.vsw = 3;
        lcdltdc.hbp = 140;
        lcdltdc.vbp = 20;
        lcdltdc.hfp = 160;
        lcdltdc.vfp = 12;
        ltdc_clk_set(300, 25, 6);
    }
    else if (lcdid == 0X7018)
    {
        lcdltdc.pwidth = 1280;
        lcdltdc.pheight = 800;

    }
    else if (lcdid == 0X4384)
    {
        lcdltdc.pwidth = 800;
        lcdltdc.pheight = 480;
        lcdltdc.hbp = 88;
        lcdltdc.hfp = 40;
        lcdltdc.hsw = 48;
        lcdltdc.vbp = 32;
        lcdltdc.vfp = 13;
        lcdltdc.vsw = 3;
        ltdc_clk_set(300, 25, 9);
    }
    else if (lcdid == 0X1018)
    {
        lcdltdc.pwidth = 1280;
        lcdltdc.pheight = 800;
        lcdltdc.hbp = 140;
        lcdltdc.hfp = 10;
        lcdltdc.hsw = 10;
        lcdltdc.vbp = 10;
        lcdltdc.vfp = 10;
        lcdltdc.vsw = 3;
        ltdc_clk_set(300, 25, 6);
    }

    if (lcdid == 0X1018)
    {
        tempreg = 1 << 28;
    }else
    {
        tempreg = 0 << 28;
    }

    tempreg |= 0 << 29;
    tempreg |= 0 << 30;
    tempreg |= 0 << 31;
    LTDC->GCR = tempreg;
    tempreg = (lcdltdc.vsw - 1) << 0;
    tempreg |= (lcdltdc.hsw - 1) << 16;
    LTDC->SSCR = tempreg;

    tempreg = (lcdltdc.vsw + lcdltdc.vbp - 1) << 0;
    tempreg |= (lcdltdc.hsw + lcdltdc.hbp - 1) << 16;
    LTDC->BPCR = tempreg;

    tempreg = (lcdltdc.vsw + lcdltdc.vbp + lcdltdc.pheight - 1) << 0;
    tempreg |= (lcdltdc.hsw + lcdltdc.hbp + lcdltdc.pwidth - 1) << 16;
    LTDC->AWCR = tempreg;

    tempreg = (lcdltdc.vsw + lcdltdc.vbp + lcdltdc.pheight + lcdltdc.vfp - 1) << 0;
    tempreg |= (lcdltdc.hsw + lcdltdc.hbp + lcdltdc.pwidth + lcdltdc.hfp - 1) << 16;
    LTDC->TWCR = tempreg;

    LTDC->BCCR = LTDC_BACKLAYERCOLOR;
    ltdc_switch(1);

#if LTDC_PIXFORMAT == LTDC_PIXFORMAT_ARGB8888 || LTDC_PIXFORMAT == LTDC_PIXFORMAT_RGB888
    g_ltdc_framebuf[0] = (uint32_t *)&ltdc_lcd_framebuf;
    lcdltdc.pixsize = 4;
#else
    g_ltdc_framebuf[0] = (uint32_t *)&ltdc_lcd_framebuf;

    lcdltdc.pixsize = 2;
#endif

    ltdc_layer_parameter_config(0, (uint32_t)g_ltdc_framebuf[0], LTDC_PIXFORMAT, 255, 0, 6, 7, 0X000000);
    ltdc_layer_window_config(0, 0, 0, lcdltdc.pwidth, lcdltdc.pheight);

    lcddev.width = lcdltdc.pwidth;
    lcddev.height = lcdltdc.pheight;

    ltdc_select_layer(0);
    LTDC_BL(1);
    ltdc_clear(0XFFFFFFFF);
}
