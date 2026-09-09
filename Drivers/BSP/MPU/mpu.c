/**
 * @file mpu.c
 * @brief 配置 Cortex-M7 MPU 区域属性，保障缓存和外设内存访问一致性。
 * @details 这是 mpu 模块的实现文件（Drivers/BSP/MPU/mpu.c）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include "./BSP/LED/led.h"
#include "./BSP/MPU/mpu.h"
#include "app_fault.h"

/**
 * @brief mpu_disable：停止或禁用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void mpu_disable(void)
{
    SCB->SHCSR &= ~(1 << 16);
    MPU->CTRL &= ~(1 << 0);
}

/**
 * @brief mpu_enable：启动或启用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void mpu_enable(void)
{
    MPU->CTRL = (1 << 2) | (1 << 0);
    SCB->SHCSR |= 1 << 16;
}

/**
 * @brief mpu_convert_bytes_to_pot：将输入值转换为调用方所需的数据格式或表示形式。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param nbytes 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint8_t mpu_convert_bytes_to_pot(uint32_t nbytes)
{
    uint8_t count = 0;

    while (nbytes != 1)
    {
        nbytes >>= 1;
        count++;
    }

    return count;
}

/**
 * @brief mpu_set_protection：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
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
uint8_t mpu_set_protection(uint32_t baseaddr, uint32_t size, uint32_t rnum, uint8_t de, uint8_t ap, uint8_t sen, uint8_t cen, uint8_t ben)
{
    uint32_t tempreg = 0;
    uint8_t rnr = 0;

    if ((size % 32) || size == 0)return 1;

    rnr = mpu_convert_bytes_to_pot(size) - 1;
    mpu_disable();
    MPU->RNR = rnum;
    MPU->RBAR = baseaddr;
    tempreg |= ((uint32_t)de) << 28;
    tempreg |= ((uint32_t)ap) << 24;
    tempreg |= 0 << 19;
    tempreg |= ((uint32_t)sen) << 18;
    tempreg |= ((uint32_t)cen) << 17;
    tempreg |= ((uint32_t)ben) << 16;
    tempreg |= 0 << 8;
    tempreg |= rnr << 1;
    tempreg |= 1 << 0;
    MPU->RASR = tempreg;
    mpu_enable();
    return 0;
}

/**
 * @brief mpu_memory_protection：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void mpu_memory_protection(void)
{

    mpu_set_protection(0x20000000, 128 * 1024, 1, 0, MPU_REGION_FULL_ACCESS, 0, 1, 1);

    mpu_set_protection(0x24000000, 512 * 1024, 2, 0, MPU_REGION_FULL_ACCESS, 0, 1, 1);

    mpu_set_protection(0x30000000, 512 * 1024, 3, 0, MPU_REGION_FULL_ACCESS, 0, 1, 1);

    mpu_set_protection(0x38000000, 64 * 1024, 4, 0, MPU_REGION_FULL_ACCESS, 0, 1, 1);

    mpu_set_protection(0x60000000, 64 * 1024 * 1024, 5, 0, MPU_REGION_FULL_ACCESS, 0, 0, 0);

    mpu_set_protection(0XC0000000, 32 * 1024 * 1024, 6, 0, MPU_REGION_FULL_ACCESS, 1, 0, 0);

    mpu_set_protection(0X80000000, 256 * 1024 * 1024, 7, 1, MPU_REGION_FULL_ACCESS, 0, 0, 0);
}

/**
 * @brief MemManage_Handler：响应中断或异步回调，完成必要的数据转移和状态通知。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
__attribute__((naked)) void MemManage_Handler(void)
{
    __asm volatile(
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "mov r1, lr\n"
        "movs r2, #7\n"
        "b app_fault_exception_frame\n");
}
