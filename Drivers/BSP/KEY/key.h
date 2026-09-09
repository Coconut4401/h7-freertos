/**
 * @file key.h
 * @brief 初始化板载按键 GPIO 并提供带消抖的按键扫描结果。
 * @details 这是 key 模块的接口文件（Drivers/BSP/KEY/key.h）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#ifndef __KEY_H
/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define __KEY_H

#include "./SYSTEM/sys/sys.h"

#define KEY0_GPIO_PORT                  GPIOH
#define KEY0_GPIO_PIN                   SYS_GPIO_PIN3
#define KEY0_GPIO_CLK_ENABLE()          do{ RCC->AHB4ENR |= 1 << 7; }while(0)

#define KEY1_GPIO_PORT                  GPIOH
#define KEY1_GPIO_PIN                   SYS_GPIO_PIN2
#define KEY1_GPIO_CLK_ENABLE()          do{ RCC->AHB4ENR |= 1 << 7; }while(0)

#define KEY2_GPIO_PORT                  GPIOC
#define KEY2_GPIO_PIN                   SYS_GPIO_PIN13
#define KEY2_GPIO_CLK_ENABLE()          do{ RCC->AHB4ENR |= 1 << 2; }while(0)

#define WKUP_GPIO_PORT                  GPIOA
#define WKUP_GPIO_PIN                   SYS_GPIO_PIN0
#define WKUP_GPIO_CLK_ENABLE()          do{ RCC->AHB4ENR |= 1 << 0; }while(0)

#define KEY0        sys_gpio_pin_get(KEY0_GPIO_PORT, KEY0_GPIO_PIN)
#define KEY1        sys_gpio_pin_get(KEY1_GPIO_PORT, KEY1_GPIO_PIN)
#define KEY2        sys_gpio_pin_get(KEY2_GPIO_PORT, KEY2_GPIO_PIN)
#define WK_UP       sys_gpio_pin_get(WKUP_GPIO_PORT, WKUP_GPIO_PIN)

#define KEY0_PRES    1
#define KEY1_PRES    2
#define KEY2_PRES    3
#define WKUP_PRES    4

/**
 * @brief key_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void key_init(void);
/**
 * @brief key_scan：扫描或采样当前输入与设备状态，整理本轮可用数据。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param mode 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t key_scan(uint8_t mode);

#endif
