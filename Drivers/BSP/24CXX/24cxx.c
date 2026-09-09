/**
 * @file 24cxx.c
 * @brief 通过软件 I2C 驱动 AT24Cxx EEPROM，提供字节和块读写接口。
 * @details 这是 24cxx 模块的实现文件（Drivers/BSP/24CXX/24cxx.c）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#include "./BSP/IIC/myiic.h"
#include "./BSP/24CXX/24cxx.h"
#include "./SYSTEM/delay/delay.h"

/**
 * @brief at24cxx_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void at24cxx_init(void)
{
    iic_init();
}

/**
 * @brief at24cxx_read_one_byte：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param addr 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t at24cxx_read_one_byte(uint16_t addr)
{
    uint8_t temp = 0;
    iic_start();

    if (EE_TYPE > AT24C16)
    {
        iic_send_byte(0XA0);
        iic_wait_ack();
        iic_send_byte(addr >> 8);
    }
    else
    {
        iic_send_byte(0XA0 + ((addr >> 8) << 1));
    }

    iic_wait_ack();
    iic_send_byte(addr % 256);
    iic_wait_ack();

    iic_start();
    iic_send_byte(0XA1);
    iic_wait_ack();
    temp = iic_read_byte(0);
    iic_stop();
    return temp;
}

/**
 * @brief at24cxx_write_one_byte：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param addr 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param data 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void at24cxx_write_one_byte(uint16_t addr, uint8_t data)
{

    iic_start();

    if (EE_TYPE > AT24C16)
    {
        iic_send_byte(0XA0);
        iic_wait_ack();
        iic_send_byte(addr >> 8);
    }
    else
    {
        iic_send_byte(0XA0 + ((addr >> 8) << 1));
    }

    iic_wait_ack();
    iic_send_byte(addr % 256);
    iic_wait_ack();

    iic_send_byte(data);
    iic_wait_ack();
    iic_stop();
    delay_ms(10);
}

/**
 * @brief at24cxx_check：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t at24cxx_check(void)
{
    uint8_t temp;
    uint16_t addr = EE_TYPE;
    temp = at24cxx_read_one_byte(addr);

    if (temp == 0X55)
    {
        return 0;
    }
    else
    {
        at24cxx_write_one_byte(addr, 0X55);
        temp = at24cxx_read_one_byte(255);

        if (temp == 0X55)return 0;
    }

    return 1;
}

/**
 * @brief at24cxx_read：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param addr 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pbuf 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param datalen 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void at24cxx_read(uint16_t addr, uint8_t *pbuf, uint16_t datalen)
{
    while (datalen--)
    {
        *pbuf++ = at24cxx_read_one_byte(addr++);
    }
}

/**
 * @brief at24cxx_write：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param addr 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param pbuf 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param datalen 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void at24cxx_write(uint16_t addr, uint8_t *pbuf, uint16_t datalen)
{
    while (datalen--)
    {
        at24cxx_write_one_byte(addr, *pbuf);
        addr++;
        pbuf++;
    }
}
