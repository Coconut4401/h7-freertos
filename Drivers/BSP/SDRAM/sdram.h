/**
 * @file sdram.h
 * @brief 初始化 FMC SDRAM 时序并发送器件上电配置命令。
 * @details 这是 sdram 模块的接口文件（Drivers/BSP/SDRAM/sdram.h）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#ifndef _SDRAM_H
/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define _SDRAM_H

#include "./SYSTEM/sys/sys.h"

#define BANK5_SDRAM_ADDR        ((uint32_t)(0XC0000000))

/**
 * @brief sdram_send_cmd：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param bankx 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param cmd 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param refresh 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param regval 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t sdram_send_cmd(uint8_t bankx, uint8_t cmd, uint8_t refresh, uint16_t regval);
/**
 * @brief sdram_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void sdram_init(void);
/**
 * @brief fmc_sdram_write_buffer：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param pbuf 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param writeaddr 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param n 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void fmc_sdram_write_buffer(uint8_t *pbuf, uint32_t writeaddr, uint32_t n);
/**
 * @brief fmc_sdram_read_buffer：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param pbuf 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param readaddr 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param n 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void fmc_sdram_read_buffer(uint8_t *pbuf, uint32_t readaddr, uint32_t n);

#endif
