/**
 * @file mpu.h
 * @brief 配置 Cortex-M7 MPU 区域属性，保障缓存和外设内存访问一致性。
 * @details 这是 mpu 模块的接口文件（Drivers/BSP/MPU/mpu.h）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#ifndef __MPU_H
/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define __MPU_H

#include "./SYSTEM/sys/sys.h"

#define  MPU_REGION_NO_ACCESS       ((uint8_t)0x00U)
#define  MPU_REGION_PRIV_RW         ((uint8_t)0x01U)
#define  MPU_REGION_PRIV_RW_URO     ((uint8_t)0x02U)
#define  MPU_REGION_FULL_ACCESS     ((uint8_t)0x03U)
#define  MPU_REGION_PRIV_RO         ((uint8_t)0x05U)
#define  MPU_REGION_PRIV_RO_URO     ((uint8_t)0x06U)

/**
 * @brief mpu_convert_bytes_to_pot：将输入值转换为调用方所需的数据格式或表示形式。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param nbytes 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint8_t mpu_convert_bytes_to_pot(uint32_t nbytes);

/**
 * @brief mpu_disable：停止或禁用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void mpu_disable(void);
/**
 * @brief mpu_enable：启动或启用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void mpu_enable(void);
/**
 * @brief mpu_memory_protection：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void mpu_memory_protection(void);
/**
 * @brief mpu_set_protection：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param baseaddr 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param size 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param rnum 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param de 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param ap 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param sen 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param cen 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param ben 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t mpu_set_protection(uint32_t baseaddr, uint32_t size, uint32_t rnum, uint8_t de, uint8_t ap, uint8_t sen, uint8_t cen, uint8_t ben);

#endif
