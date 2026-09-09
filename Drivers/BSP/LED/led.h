/**
 * @file led.h
 * @brief 初始化并控制板载 LED GPIO。
 * @details 这是 led 模块的接口文件（Drivers/BSP/LED/led.h）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#ifndef __LED_H
/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define __LED_H

#include "./SYSTEM/sys/sys.h"

#define LED0_GPIO_PORT                  GPIOB
#define LED0_GPIO_PIN                   SYS_GPIO_PIN1
#define LED0_GPIO_CLK_ENABLE()          do{ RCC->AHB4ENR |= 1 << 1; }while(0)

#define LED1_GPIO_PORT                  GPIOB
#define LED1_GPIO_PIN                   SYS_GPIO_PIN0
#define LED1_GPIO_CLK_ENABLE()          do{ RCC->AHB4ENR |= 1 << 1; }while(0)

#define LED0(x)         sys_gpio_pin_set(LED0_GPIO_PORT, LED0_GPIO_PIN, x)
#define LED1(x)         sys_gpio_pin_set(LED1_GPIO_PORT, LED1_GPIO_PIN, x)

#define LED0_TOGGLE()   do{ LED0_GPIO_PORT->ODR^=LED0_GPIO_PIN; }while(0)
#define LED1_TOGGLE()   do{ LED1_GPIO_PORT->ODR^=LED1_GPIO_PIN; }while(0)

/**
 * @brief led_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void led_init(void);

#endif
