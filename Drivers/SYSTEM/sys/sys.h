/**
 * @file sys.h
 * @brief 封装时钟、GPIO、缓存和底层系统配置操作。
 * @details 这是 sys 模块的接口文件（Drivers/SYSTEM/sys/sys.h）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#ifndef __SYS_H
/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define __SYS_H

#include "stm32h7xx.h"

#define SYS_SUPPORT_OS          0

#define SYS_GPIO_FTIR           1
#define SYS_GPIO_RTIR           2
#define SYS_GPIO_BTIR           3

#define SYS_GPIO_MODE_IN        0
#define SYS_GPIO_MODE_OUT       1
#define SYS_GPIO_MODE_AF        2
#define SYS_GPIO_MODE_AIN       3

#define SYS_GPIO_SPEED_LOW      0
#define SYS_GPIO_SPEED_MID      1
#define SYS_GPIO_SPEED_FAST     2
#define SYS_GPIO_SPEED_HIGH     3

#define SYS_GPIO_PUPD_NONE      0
#define SYS_GPIO_PUPD_PU        1
#define SYS_GPIO_PUPD_PD        2
#define SYS_GPIO_PUPD_RES       3

#define SYS_GPIO_OTYPE_PP       0
#define SYS_GPIO_OTYPE_OD       1

#define SYS_GPIO_PIN0           1<<0
#define SYS_GPIO_PIN1           1<<1
#define SYS_GPIO_PIN2           1<<2
#define SYS_GPIO_PIN3           1<<3
#define SYS_GPIO_PIN4           1<<4
#define SYS_GPIO_PIN5           1<<5
#define SYS_GPIO_PIN6           1<<6
#define SYS_GPIO_PIN7           1<<7
#define SYS_GPIO_PIN8           1<<8
#define SYS_GPIO_PIN9           1<<9
#define SYS_GPIO_PIN10          1<<10
#define SYS_GPIO_PIN11          1<<11
#define SYS_GPIO_PIN12          1<<12
#define SYS_GPIO_PIN13          1<<13
#define SYS_GPIO_PIN14          1<<14
#define SYS_GPIO_PIN15          1<<15

/**
 * @brief sys_nvic_priority_group_config：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param group 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void sys_nvic_priority_group_config(uint8_t group);

/**
 * @brief sys_nvic_set_vector_table：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param baseaddr 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param offset 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void sys_nvic_set_vector_table(uint32_t baseaddr, uint32_t offset);
/**
 * @brief sys_nvic_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param pprio 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param sprio 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param ch 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param group 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void sys_nvic_init(uint8_t pprio, uint8_t sprio, uint8_t ch, uint8_t group);
/**
 * @brief sys_nvic_ex_config：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param p_gpiox 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pinx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param tmode 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void sys_nvic_ex_config(GPIO_TypeDef *p_gpiox, uint16_t pinx, uint8_t tmode);
/**
 * @brief sys_gpio_af_set：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param gpiox 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pinx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param afx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void sys_gpio_af_set(GPIO_TypeDef *gpiox, uint16_t pinx, uint8_t afx);
/**
 * @brief sys_gpio_set：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param p_gpiox 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pinx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param mode 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param otype 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param ospeed 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pupd 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void sys_gpio_set(GPIO_TypeDef *p_gpiox, uint16_t pinx, uint32_t mode,
                  uint32_t otype, uint32_t ospeed, uint32_t pupd);
/**
 * @brief sys_gpio_pin_set：将指定 GPIO 引脚设置为高电平或低电平。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param p_gpiox 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pinx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param status 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void sys_gpio_pin_set(GPIO_TypeDef *p_gpiox, uint16_t pinx, uint8_t status);
/**
 * @brief sys_gpio_pin_get：读取指定 GPIO 输入数据寄存器并返回引脚逻辑电平。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param p_gpiox 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pinx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t sys_gpio_pin_get(GPIO_TypeDef *p_gpiox, uint16_t pinx);
/**
 * @brief sys_standby：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void sys_standby(void);
/**
 * @brief sys_soft_reset：清除已有状态或复位目标设备，使其回到约定的初始状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void sys_soft_reset(void);
/**
 * @brief sys_cache_enable：启动或启用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void sys_cache_enable(void);
/**
 * @brief sys_clock_set：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param plln 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pllm 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pllp 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pllq 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t sys_clock_set(uint32_t plln, uint32_t pllm, uint32_t pllp, uint32_t pllq);
/**
 * @brief sys_stm32_clock_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param plln 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pllm 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pllp 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pllq 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void sys_stm32_clock_init(uint32_t plln, uint32_t pllm, uint32_t pllp, uint32_t pllq);

/**
 * @brief sys_wfi_set：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void sys_wfi_set(void);
/**
 * @brief sys_intx_disable：停止或禁用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void sys_intx_disable(void);
/**
 * @brief sys_intx_enable：启动或启用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void sys_intx_enable(void);
/**
 * @brief sys_msr_msp：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param addr 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void sys_msr_msp(uint32_t addr);

#endif
