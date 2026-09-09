/**
 * @file 24cxx.h
 * @brief 通过软件 I2C 驱动 AT24Cxx EEPROM，提供字节和块读写接口。
 * @details 这是 24cxx 模块的接口文件（Drivers/BSP/24CXX/24cxx.h）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#ifndef __24CXX_H
/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define __24CXX_H

#include "./SYSTEM/sys/sys.h"

#define AT24C01     127
#define AT24C02     255
#define AT24C04     511
#define AT24C08     1023
#define AT24C16     2047
#define AT24C32     4095
#define AT24C64     8191
#define AT24C128    16383
#define AT24C256    32767

#define EE_TYPE     AT24C02

/**
 * @brief at24cxx_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void at24cxx_init(void);
/**
 * @brief at24cxx_check：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t at24cxx_check(void);
/**
 * @brief at24cxx_read_one_byte：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param addr 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t at24cxx_read_one_byte(uint16_t addr);
/**
 * @brief at24cxx_write_one_byte：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param addr 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param data 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void at24cxx_write_one_byte(uint16_t addr,uint8_t data);
/**
 * @brief at24cxx_write：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param addr 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pbuf 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param datalen 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void at24cxx_write(uint16_t addr, uint8_t *pbuf, uint16_t datalen);
/**
 * @brief at24cxx_read：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param addr 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pbuf 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param datalen 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void at24cxx_read(uint16_t addr, uint8_t *pbuf, uint16_t datalen);

#endif
