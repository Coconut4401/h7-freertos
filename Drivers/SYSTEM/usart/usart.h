/**
 * @file usart.h
 * @brief 初始化调试串口，处理接收数据并提供标准输出重定向。
 * @details 这是 usart 模块的接口文件（Drivers/SYSTEM/usart/usart.h）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#ifndef __USART_H
/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define __USART_H

#include "stdio.h"
#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart_rx.h"

#define USART_TX_GPIO_PORT                  GPIOA
#define USART_TX_GPIO_PIN                   SYS_GPIO_PIN9
#define USART_TX_GPIO_AF                    7
#define USART_TX_GPIO_CLK_ENABLE()          do{ RCC->AHB4ENR |= 1 << 0; }while(0)

#define USART_RX_GPIO_PORT                  GPIOA
#define USART_RX_GPIO_PIN                   SYS_GPIO_PIN10
#define USART_RX_GPIO_AF                    7
#define USART_RX_GPIO_CLK_ENABLE()          do{ RCC->AHB4ENR |= 1 << 0; }while(0)

#define USART_UX                            USART1
#define USART_UX_IRQn                       USART1_IRQn
#define USART_UX_IRQHandler                 USART1_IRQHandler
#define USART_UX_CLK_ENABLE()               do{ RCC->APB2ENR |= 1 << 4; }while(0)

#define USART_RX_RING_SIZE          256U
#define USART_EN_RX                 1

/**
 * @brief usart_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param pclk2 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param bound 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void usart_init(uint32_t pclk2, uint32_t bound);
/**
 * @brief usart_tx_write：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param data 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param length 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void usart_tx_write(const uint8_t *data, uint16_t length);

#endif
