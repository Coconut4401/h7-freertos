/**
 * @file touch.h
 * @brief 统一电阻屏与电容屏初始化、校准、扫描和坐标状态。
 * @details 这是 touch 模块的接口文件（Drivers/BSP/TOUCH/touch.h）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#ifndef __TOUCH_H
/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define __TOUCH_H

#include "./SYSTEM/sys/sys.h"
#include "./BSP/TOUCH/gt9xxx.h"
#include "./BSP/TOUCH/ft5206.h"

#define T_PEN_GPIO_PORT                 GPIOH
#define T_PEN_GPIO_PIN                  SYS_GPIO_PIN7
#define T_PEN_GPIO_CLK_ENABLE()         do{ RCC->AHB4ENR |= 1 << 7; }while(0)

#define T_CS_GPIO_PORT                  GPIOI
#define T_CS_GPIO_PIN                   SYS_GPIO_PIN8
#define T_CS_GPIO_CLK_ENABLE()          do{ RCC->AHB4ENR |= 1 << 8; }while(0)

#define T_MISO_GPIO_PORT                GPIOG
#define T_MISO_GPIO_PIN                 SYS_GPIO_PIN3
#define T_MISO_GPIO_CLK_ENABLE()        do{ RCC->AHB4ENR |= 1 << 6; }while(0)

#define T_MOSI_GPIO_PORT                GPIOI
#define T_MOSI_GPIO_PIN                 SYS_GPIO_PIN3
#define T_MOSI_GPIO_CLK_ENABLE()        do{ RCC->AHB4ENR |= 1 << 8; }while(0)

#define T_CLK_GPIO_PORT                 GPIOH
#define T_CLK_GPIO_PIN                  SYS_GPIO_PIN6
#define T_CLK_GPIO_CLK_ENABLE()         do{ RCC->AHB4ENR |= 1 << 7; }while(0)

#define T_PEN           sys_gpio_pin_get(T_PEN_GPIO_PORT, T_PEN_GPIO_PIN)
#define T_MISO          sys_gpio_pin_get(T_MISO_GPIO_PORT, T_MISO_GPIO_PIN)

#define T_MOSI(x)       sys_gpio_pin_set(T_MOSI_GPIO_PORT, T_MOSI_GPIO_PIN, x)
#define T_CLK(x)        sys_gpio_pin_set(T_CLK_GPIO_PORT, T_CLK_GPIO_PIN, x)
#define T_CS(x)         sys_gpio_pin_set(T_CS_GPIO_PORT, T_CS_GPIO_PIN, x)

#define TP_PRES_DOWN    0x8000
#define TP_CATH_PRES    0x4000
#define CT_MAX_TOUCH    10

/** @brief 模块数据类型：描述本模块维护的状态、配置或数据快照。 */
typedef struct
{
    uint8_t (*init)(void);
    uint8_t (*scan)(uint8_t);
    void (*adjust)(void);
    uint16_t x[CT_MAX_TOUCH];
    uint16_t y[CT_MAX_TOUCH];

    uint16_t sta;

    float xfac;
    float yfac;
    short xc;
    short yc;

    uint8_t touchtype;
} _m_tp_dev;

extern _m_tp_dev tp_dev;

/**
 * @brief tp_write_byte：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param data 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void tp_write_byte(uint8_t data);
/**
 * @brief tp_read_ad：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param cmd 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint16_t tp_read_ad(uint8_t cmd);
/**
 * @brief tp_read_xoy：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param cmd 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint16_t tp_read_xoy(uint8_t cmd);
/**
 * @brief tp_read_xy：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void tp_read_xy(uint16_t *x, uint16_t *y);
/**
 * @brief tp_read_xy2：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint8_t tp_read_xy2(uint16_t *x, uint16_t *y);
/**
 * @brief tp_draw_touch_point：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void tp_draw_touch_point(uint16_t x, uint16_t y, uint16_t color);
/**
 * @brief tp_adjust_info_show：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param px 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param py 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void tp_adjust_info_show(uint16_t xy[5][2], double px, double py);

/**
 * @brief tp_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t tp_init(void);
/**
 * @brief tp_scan：扫描或采样当前输入与设备状态，整理本轮可用数据。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param mode 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t tp_scan(uint8_t mode);
/**
 * @brief tp_adjust：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void tp_adjust(void);
/**
 * @brief tp_save_adjust_data：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void tp_save_adjust_data(void);
/**
 * @brief tp_get_adjust_data：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t tp_get_adjust_data(void);
/**
 * @brief tp_draw_big_point：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void tp_draw_big_point(uint16_t x, uint16_t y, uint16_t color);

#endif
