/**
 * @file gt9xxx.h
 * @brief 驱动 GT9xxx 电容触摸控制器，完成地址探测和触点读取。
 * @details 这是 gt9xxx 模块的接口文件（Drivers/BSP/TOUCH/gt9xxx.h）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#ifndef __GT9XXX_H
/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define __GT9XXX_H

#include "./SYSTEM/sys/sys.h"

#define GT9XXX_RST_GPIO_PORT            GPIOI
#define GT9XXX_RST_GPIO_PIN             SYS_GPIO_PIN8
#define GT9XXX_RST_GPIO_CLK_ENABLE()    do{ RCC->AHB4ENR |= 1 << 8; }while(0)

#define GT9XXX_INT_GPIO_PORT            GPIOH
#define GT9XXX_INT_GPIO_PIN             SYS_GPIO_PIN7
#define GT9XXX_INT_GPIO_CLK_ENABLE()    do{ RCC->AHB4ENR |= 1 << 7; }while(0)

#define GT9XXX_RST(x)   sys_gpio_pin_set(GT9XXX_RST_GPIO_PORT, GT9XXX_RST_GPIO_PIN, x)
#define GT9XXX_INT      sys_gpio_pin_get(GT9XXX_INT_GPIO_PORT, GT9XXX_INT_GPIO_PIN)

#define GT9XXX_ADDR_14_WR   0X28
#define GT9XXX_ADDR_14_RD   0X29
#define GT9XXX_ADDR_5D_WR   0XBA
#define GT9XXX_ADDR_5D_RD   0XBB

#define GT9XXX_CTRL_REG     0X8040
#define GT9XXX_CFGS_REG     0X8047
#define GT9XXX_CHECK_REG    0X80FF
#define GT9XXX_PID_REG      0X8140

#define GT9XXX_GSTID_REG    0X814E
#define GT9XXX_TP1_REG      0X8150
#define GT9XXX_TP2_REG      0X8158
#define GT9XXX_TP3_REG      0X8160
#define GT9XXX_TP4_REG      0X8168
#define GT9XXX_TP5_REG      0X8170
#define GT9XXX_TP6_REG      0X8178
#define GT9XXX_TP7_REG      0X8180
#define GT9XXX_TP8_REG      0X8188
#define GT9XXX_TP9_REG      0X8190
#define GT9XXX_TP10_REG     0X8198

/**
 * @brief gt9xxx_wr_reg：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param reg 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param buf 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param len 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t gt9xxx_wr_reg(uint16_t reg,uint8_t *buf,uint8_t len);
/**
 * @brief gt9xxx_rd_reg：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param reg 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param buf 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param len 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void gt9xxx_rd_reg(uint16_t reg,uint8_t *buf,uint8_t len);
/**
 * @brief gt9xxx_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t gt9xxx_init(void);
/**
 * @brief gt9xxx_get_i2c_address：返回当前探测成功的 GT9xxx 七位 I2C 地址。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t gt9xxx_get_i2c_address(void);
/**
 * @brief gt9xxx_scan：扫描或采样当前输入与设备状态，整理本轮可用数据。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param mode 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t gt9xxx_scan(uint8_t mode);
#endif
