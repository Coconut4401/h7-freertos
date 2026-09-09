/**
 * @file usart_rx.h
 * @brief 声明调试串口接收缓冲区及其共享状态。
 * @details 这是 usart_rx 模块的接口文件（Drivers/SYSTEM/usart/usart_rx.h）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#ifndef USART_RX_H
/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define USART_RX_H

#include <stdint.h>

/**
 * @brief usart_rx_read_byte：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param data 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t usart_rx_read_byte(uint8_t *data);
/**
 * @brief usart_rx_reset：清除已有状态或复位目标设备，使其回到约定的初始状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void usart_rx_reset(void);
/**
 * @brief usart_rx_get_dropped_count：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint32_t usart_rx_get_dropped_count(void);

#endif
