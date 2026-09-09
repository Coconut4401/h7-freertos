/**
 * @file myiic.h
 * @brief 以 GPIO 模拟通用 I2C 主机时序，供板载低速器件使用。
 * @details 这是 myiic 模块的接口文件（Drivers/BSP/IIC/myiic.h）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#ifndef __MYIIC_H
/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define __MYIIC_H

#include "./SYSTEM/sys/sys.h"

#define IIC_SCL_GPIO_PORT               GPIOH
#define IIC_SCL_GPIO_PIN                SYS_GPIO_PIN4
#define IIC_SCL_GPIO_CLK_ENABLE()       do{ RCC->AHB4ENR |= 1 << 7; }while(0)

#define IIC_SDA_GPIO_PORT               GPIOH
#define IIC_SDA_GPIO_PIN                SYS_GPIO_PIN5
#define IIC_SDA_GPIO_CLK_ENABLE()       do{ RCC->AHB4ENR |= 1 << 7; }while(0)

#define IIC_SCL(x)      sys_gpio_pin_set(IIC_SCL_GPIO_PORT, IIC_SCL_GPIO_PIN, x)
#define IIC_SDA(x)      sys_gpio_pin_set(IIC_SDA_GPIO_PORT, IIC_SDA_GPIO_PIN, x)
#define IIC_READ_SDA    sys_gpio_pin_get(IIC_SDA_GPIO_PORT, IIC_SDA_GPIO_PIN)

/**
 * @brief iic_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void iic_init(void);
/**
 * @brief iic_start：启动或启用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void iic_start(void);
/**
 * @brief iic_stop：停止或禁用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void iic_stop(void);
/**
 * @brief iic_ack：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void iic_ack(void);
/**
 * @brief iic_nack：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void iic_nack(void);
/**
 * @brief iic_wait_ack：等待指定时长或硬件条件，以满足总线时序与同步要求。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t iic_wait_ack(void);
/**
 * @brief iic_send_byte：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param data 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void iic_send_byte(uint8_t data);
/**
 * @brief iic_read_byte：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param ack 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t iic_read_byte(uint8_t ack);
#endif
