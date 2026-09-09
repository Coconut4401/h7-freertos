/**
 * @file ctiic.h
 * @brief 以 GPIO 模拟电容触摸控制器使用的 I2C 总线时序。
 * @details 这是 ctiic 模块的接口文件（Drivers/BSP/TOUCH/ctiic.h）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#ifndef __CTIIC_H
/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define __CTIIC_H

#include "./SYSTEM/sys/sys.h"

#define CT_IIC_SCL_GPIO_PORT            GPIOH
#define CT_IIC_SCL_GPIO_PIN             SYS_GPIO_PIN6
#define CT_IIC_SCL_GPIO_CLK_ENABLE()    do{ RCC->AHB4ENR |= 1 << 7; }while(0)

#define CT_IIC_SDA_GPIO_PORT            GPIOI
#define CT_IIC_SDA_GPIO_PIN             SYS_GPIO_PIN3
#define CT_IIC_SDA_GPIO_CLK_ENABLE()    do{ RCC->AHB4ENR |= 1 << 8; }while(0)

#define CT_IIC_SCL(x)      sys_gpio_pin_set(CT_IIC_SCL_GPIO_PORT, CT_IIC_SCL_GPIO_PIN, x)
#define CT_IIC_SDA(x)      sys_gpio_pin_set(CT_IIC_SDA_GPIO_PORT, CT_IIC_SDA_GPIO_PIN, x)
#define CT_READ_SDA        sys_gpio_pin_get(CT_IIC_SDA_GPIO_PORT, CT_IIC_SDA_GPIO_PIN)

/**
 * @brief ct_iic_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void ct_iic_init(void);
/**
 * @brief ct_iic_stop：停止或禁用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void ct_iic_stop(void);
/**
 * @brief ct_iic_start：启动或启用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void ct_iic_start(void);

/**
 * @brief ct_iic_ack：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void ct_iic_ack(void);
/**
 * @brief ct_iic_nack：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void ct_iic_nack(void);
/**
 * @brief ct_iic_wait_ack：等待指定时长或硬件条件，以满足总线时序与同步要求。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t ct_iic_wait_ack(void);

/**
 * @brief ct_iic_send_byte：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param txd 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void ct_iic_send_byte(uint8_t txd);
/**
 * @brief ct_iic_read_byte：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param ack 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t ct_iic_read_byte(unsigned char ack);
/**
 * @brief ct_iic_check_device：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param address 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t ct_iic_check_device(uint8_t address);

#endif
