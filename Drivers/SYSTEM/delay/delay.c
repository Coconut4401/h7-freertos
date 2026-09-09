/**
 * @file delay.c
 * @brief 基于 SysTick 或内核节拍提供微秒、毫秒级延时。
 * @details 这是 delay 模块的实现文件（Drivers/SYSTEM/delay/delay.c）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/delay/delay.h"

static uint32_t g_fac_us = 0;

#if SYS_SUPPORT_OS

#include "os.h"

static uint16_t g_fac_ms = 0;

/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define delay_osrunning     OSRunning
#define delay_ostickspersec OS_TICKS_PER_SEC
#define delay_osintnesting  OSIntNesting

/**
 * @brief delay_osschedlock：等待指定时长或硬件条件，以满足总线时序与同步要求。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void delay_osschedlock(void)
{
    OSSchedLock();
}

/**
 * @brief delay_osschedunlock：等待指定时长或硬件条件，以满足总线时序与同步要求。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void delay_osschedunlock(void)
{
    OSSchedUnlock();
}

/**
 * @brief delay_ostimedly：等待指定时长或硬件条件，以满足总线时序与同步要求。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param ticks 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void delay_ostimedly(uint32_t ticks)
{
    OSTimeDly(ticks);
}

/**
 * @brief SysTick_Handler：响应中断或异步回调，完成必要的数据转移和状态通知。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void SysTick_Handler(void)
{
    if (delay_osrunning == OS_TRUE)
    {
        OS_CPU_SysTickHandler();
    }
}

#endif

/**
 * @brief delay_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param sysclk 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void delay_init(uint16_t sysclk)
{
#if SYS_SUPPORT_OS
    uint32_t reload;
#endif
    SysTick->CTRL |= (1 << 2);
    g_fac_us = sysclk;
    SysTick->CTRL |= 1 << 0;
    SysTick->LOAD = 0X0FFFFFFF;
#if SYS_SUPPORT_OS
    reload = sysclk;
    reload *= 1000000 / delay_ostickspersec;

    g_fac_ms = 1000 / delay_ostickspersec;
    SysTick->CTRL |= 1 << 1;
    SysTick->LOAD = reload;
#endif
}

/**
 * @brief delay_us：等待指定时长或硬件条件，以满足总线时序与同步要求。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param nus 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void delay_us(uint32_t nus)
{
    uint32_t ticks;
    uint32_t told, tnow, tcnt = 0;
    uint32_t reload = SysTick->LOAD;
    ticks = nus * g_fac_us;

#if SYS_SUPPORT_OS
    delay_osschedlock();
#endif

    told = SysTick->VAL;
    while (1)
    {
        tnow = SysTick->VAL;
        if (tnow != told)
        {
            if (tnow < told)
            {
                tcnt += told - tnow;
            }
            else
            {
                tcnt += reload - tnow + told;
            }
            told = tnow;
            if (tcnt >= ticks)
            {
                break;
            }
        }
    }

#if SYS_SUPPORT_OS
    delay_osschedunlock();
#endif

}

/**
 * @brief delay_ms：等待指定时长或硬件条件，以满足总线时序与同步要求。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param nms 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void delay_ms(uint16_t nms)
{

#if SYS_SUPPORT_OS
    if (delay_osrunning && delay_osintnesting == 0)
    {
        if (nms >= g_fac_ms)
        {
            delay_ostimedly(nms / g_fac_ms);
        }

        nms %= g_fac_ms;
    }
#endif

    delay_us((uint32_t)(nms * 1000));
}
