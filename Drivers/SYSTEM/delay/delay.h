/**
 * @file delay.h
 * @brief 基于 SysTick 或内核节拍提供微秒、毫秒级延时。
 * @details 这是 delay 模块的接口文件（Drivers/SYSTEM/delay/delay.h）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#ifndef __DELAY_H
/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define __DELAY_H

#include "./SYSTEM/sys/sys.h"

/**
 * @brief delay_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param sysclk 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void delay_init(uint16_t sysclk);
/**
 * @brief delay_ms：等待指定时长或硬件条件，以满足总线时序与同步要求。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param nms 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void delay_ms(uint16_t nms);
/**
 * @brief delay_us：等待指定时长或硬件条件，以满足总线时序与同步要求。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param nus 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void delay_us(uint32_t nus);

#endif
