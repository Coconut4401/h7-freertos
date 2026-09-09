/**
 * @file ft5206.h
 * @brief 驱动 FT5206 电容触摸控制器并读取多点触摸坐标。
 * @details 这是 ft5206 模块的接口文件（Drivers/BSP/TOUCH/ft5206.h）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#ifndef __FT5206_H
/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define __FT5206_H

#include "./SYSTEM/sys/sys.h"

#define FT5206_RST_GPIO_PORT            GPIOI
#define FT5206_RST_GPIO_PIN             SYS_GPIO_PIN8
#define FT5206_RST_GPIO_CLK_ENABLE()    do{ RCC->AHB4ENR |= 1 << 8; }while(0)

#define FT5206_INT_GPIO_PORT            GPIOH
#define FT5206_INT_GPIO_PIN             SYS_GPIO_PIN7
#define FT5206_INT_GPIO_CLK_ENABLE()    do{ RCC->AHB4ENR |= 1 << 7; }while(0)

#define FT5206_RST(x)   sys_gpio_pin_set(FT5206_RST_GPIO_PORT, FT5206_RST_GPIO_PIN, x)
#define FT5206_INT      sys_gpio_pin_get(FT5206_INT_GPIO_PORT, FT5206_INT_GPIO_PIN)

#define FT5206_CMD_WR               0X70
#define FT5206_CMD_RD               0X71

#define FT5206_DEVIDE_MODE          0x00
#define FT5206_REG_NUM_FINGER       0x02

#define FT5206_TP1_REG              0X03
#define FT5206_TP2_REG              0X09
#define FT5206_TP3_REG              0X0F
#define FT5206_TP4_REG              0X15
#define FT5206_TP5_REG              0X1B

#define	FT5206_ID_G_LIB_VERSION     0xA1
#define FT5206_ID_G_MODE            0xA4
#define FT5206_ID_G_THGROUP         0x80
#define FT5206_ID_G_PERIODACTIVE    0x88

/**
 * @brief ft5206_wr_reg：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param reg 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param buf 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param len 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t ft5206_wr_reg(uint16_t reg,uint8_t *buf,uint8_t len);
/**
 * @brief ft5206_rd_reg：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param reg 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param buf 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param len 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void ft5206_rd_reg(uint16_t reg,uint8_t *buf,uint8_t len);
/**
 * @brief ft5206_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t ft5206_init(void);
/**
 * @brief ft5206_scan：扫描或采样当前输入与设备状态，整理本轮可用数据。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param mode 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t ft5206_scan(uint8_t mode);

#endif
