/**
 * @file ltdc.h
 * @brief 配置 STM32 LTDC、图层和帧缓冲，并控制显示时序与混合。
 * @details 这是 ltdc 模块的接口文件（Drivers/BSP/LCD/ltdc.h）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#ifndef __LTDC_H
/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define __LTDC_H

#include "./SYSTEM/sys/sys.h"

/** @brief 模块数据类型：描述本模块维护的状态、配置或数据快照。 */
typedef struct
{
    uint32_t pwidth;
    uint32_t pheight;
    uint16_t hsw;
    uint16_t vsw;
    uint16_t hbp;
    uint16_t vbp;
    uint16_t hfp;
    uint16_t vfp;
    uint8_t activelayer;
    uint8_t dir;
    uint16_t width;
    uint16_t height;
    uint32_t pixsize;
} _ltdc_dev;

extern _ltdc_dev lcdltdc;

#define LTDC_PIXFORMAT_ARGB8888      0X00
#define LTDC_PIXFORMAT_RGB888        0X01
#define LTDC_PIXFORMAT_RGB565        0X02
#define LTDC_PIXFORMAT_ARGB1555      0X03
#define LTDC_PIXFORMAT_ARGB4444      0X04
#define LTDC_PIXFORMAT_L8            0X05
#define LTDC_PIXFORMAT_AL44          0X06
#define LTDC_PIXFORMAT_AL88          0X07

#define LTDC_BL_GPIO_PORT               GPIOB
#define LTDC_BL_GPIO_PIN                SYS_GPIO_PIN5
#define LTDC_BL_GPIO_CLK_ENABLE()       do{ RCC->AHB4ENR |= 1 << 1; }while(0)

#define LTDC_DE_GPIO_PORT               GPIOF
#define LTDC_DE_GPIO_PIN                SYS_GPIO_PIN10
#define LTDC_DE_GPIO_CLK_ENABLE()       do{ RCC->AHB4ENR |= 1 << 5; }while(0)

#define LTDC_VSYNC_GPIO_PORT            GPIOI
#define LTDC_VSYNC_GPIO_PIN             SYS_GPIO_PIN9
#define LTDC_VSYNC_GPIO_CLK_ENABLE()    do{ RCC->AHB4ENR |= 1 << 8; }while(0)

#define LTDC_HSYNC_GPIO_PORT            GPIOI
#define LTDC_HSYNC_GPIO_PIN             SYS_GPIO_PIN10
#define LTDC_HSYNC_GPIO_CLK_ENABLE()    do{ RCC->AHB4ENR |= 1 << 8; }while(0)

#define LTDC_CLK_GPIO_PORT              GPIOG
#define LTDC_CLK_GPIO_PIN               SYS_GPIO_PIN7
#define LTDC_CLK_GPIO_CLK_ENABLE()      do{ RCC->AHB4ENR |= 1 << 6; }while(0)

#define LTDC_PIXFORMAT              LTDC_PIXFORMAT_RGB565

#define LTDC_BACKLAYERCOLOR         0X00000000

#define LTDC_FRAME_BUF_ADDR         0XC0000000

#define LTDC_BL(x)                  sys_gpio_pin_set(LTDC_BL_GPIO_PORT, LTDC_BL_GPIO_PIN, x)

/**
 * @brief ltdc_switch：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param sw 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void ltdc_switch(uint8_t sw);
/**
 * @brief ltdc_layer_switch：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param layerx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param sw 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void ltdc_layer_switch(uint8_t layerx, uint8_t sw);
/**
 * @brief ltdc_select_layer：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param layerx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void ltdc_select_layer(uint8_t layerx);
/**
 * @brief ltdc_display_dir：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param dir 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void ltdc_display_dir(uint8_t dir);
/**
 * @brief ltdc_draw_point：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void ltdc_draw_point(uint16_t x, uint16_t y, uint32_t color);
/**
 * @brief ltdc_read_point：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint32_t ltdc_read_point(uint16_t x, uint16_t y);
/**
 * @brief ltdc_fill：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param sx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param sy 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param ex 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param ey 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void ltdc_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint32_t color);
/**
 * @brief ltdc_color_fill：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param sx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param sy 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param ex 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param ey 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void ltdc_color_fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color);
/**
 * @brief ltdc_clear：清除已有状态或复位目标设备，使其回到约定的初始状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void ltdc_clear(uint32_t color);
/**
 * @brief ltdc_clk_set：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param pllsain 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pllsair 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pllsaidivr 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t ltdc_clk_set(uint32_t pllsain, uint32_t pllsair, uint32_t pllsaidivr);
/**
 * @brief ltdc_layer_window_config：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param layerx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param sx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param sy 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param width 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param height 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void ltdc_layer_window_config(uint8_t layerx, uint16_t sx, uint16_t sy, uint16_t width, uint16_t height);
/**
 * @brief ltdc_layer_parameter_config：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
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
void ltdc_layer_parameter_config(uint8_t layerx, uint32_t bufaddr, uint8_t pixformat, uint8_t alpha, uint8_t alpha0, uint8_t bfac1, uint8_t bfac2, uint32_t bkcolor);
/**
 * @brief ltdc_panelid_read：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint16_t ltdc_panelid_read(void);
/**
 * @brief ltdc_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void ltdc_init(void);

#endif
