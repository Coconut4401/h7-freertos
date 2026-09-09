/**
 * @file usart.c
 * @brief 初始化调试串口，处理接收数据并提供标准输出重定向。
 * @details 这是 usart 模块的实现文件（Drivers/SYSTEM/usart/usart.c）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"

#if SYS_SUPPORT_OS
#include "os.h"
#endif

#if 1
#if (__ARMCC_VERSION >= 6010050)
__asm(".global __use_no_semihosting\n\t");
__asm(".global __ARM_use_no_argv \n\t");

#else

#pragma import(__use_no_semihosting)

struct __FILE
{
    int handle;

};

#endif

/**
 * @brief _ttywrch：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param ch 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
int _ttywrch(int ch)
{
    ch = ch;
    return ch;
}

/**
 * @brief _sys_exit：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void _sys_exit(int x)
{
    x = x;
}

/**
 * @brief _sys_command_string：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param cmd 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param len 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
char *_sys_command_string(char *cmd, int len)
{
    return NULL;
}

FILE __stdout;

/**
 * @brief fputc：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param ch 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param f 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
int fputc(int ch, FILE *f)
{
    while ((USART_UX->ISR & 0X40) == 0);

    USART_UX->TDR = (uint8_t)ch;
    return ch;
}
#endif

/**
 * @brief usart_tx_write：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param data 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param length 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void usart_tx_write(const uint8_t *data, uint16_t length)
{
    uint16_t index;

    if (data == NULL)
    {
        return;
    }

    for (index = 0U; index < length; index++)
    {
        while ((USART_UX->ISR & (1U << 7)) == 0U)
        {
        }
        USART_UX->TDR = data[index];
    }
    while ((USART_UX->ISR & (1U << 6)) == 0U)
    {
    }
}

#if USART_EN_RX

static uint8_t g_usart_rx_ring[USART_RX_RING_SIZE];
static volatile uint16_t g_usart_rx_head;
static volatile uint16_t g_usart_rx_tail;
static volatile uint32_t g_usart_rx_dropped;

/**
 * @brief usart_rx_read_byte：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param data 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t usart_rx_read_byte(uint8_t *data)
{
    uint16_t tail;

    if (data == NULL)
    {
        return 0U;
    }

    tail = g_usart_rx_tail;
    if (tail == g_usart_rx_head)
    {
        return 0U;
    }

    *data = g_usart_rx_ring[tail];
    g_usart_rx_tail = (uint16_t)((tail + 1U) &
                                 (USART_RX_RING_SIZE - 1U));
    return 1U;
}

/**
 * @brief usart_rx_reset：清除已有状态或复位目标设备，使其回到约定的初始状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void usart_rx_reset(void)
{
    g_usart_rx_tail = g_usart_rx_head;
}

/**
 * @brief usart_rx_get_dropped_count：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint32_t usart_rx_get_dropped_count(void)
{
    return g_usart_rx_dropped;
}

/**
 * @brief USART_UX_IRQHandler：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 * @warning 该入口具有特定中断或任务上下文，禁止执行不符合该上下文约束的操作。
 */
void USART_UX_IRQHandler(void)
{
    uint8_t rxdata;
    uint16_t head;
    uint16_t next;

    while (USART_UX->ISR & (1U << 5))
    {
        rxdata = USART_UX->RDR;
        head = g_usart_rx_head;
        next = (uint16_t)((head + 1U) & (USART_RX_RING_SIZE - 1U));
        if (next != g_usart_rx_tail)
        {
            g_usart_rx_ring[head] = rxdata;
            g_usart_rx_head = next;
        }
        else
        {
            g_usart_rx_dropped++;
        }
    }

    USART_UX->ICR = (1U << 3) | (1U << 2) | (1U << 1);
}
#endif

/**
 * @brief usart_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param sclk 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param baudrate 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void usart_init(uint32_t sclk, uint32_t baudrate)
{
    uint32_t temp;

    USART_TX_GPIO_CLK_ENABLE();
    USART_RX_GPIO_CLK_ENABLE();
    USART_UX_CLK_ENABLE();

    sys_gpio_set(USART_TX_GPIO_PORT, USART_TX_GPIO_PIN,
                 SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_MID, SYS_GPIO_PUPD_PU);

    sys_gpio_set(USART_RX_GPIO_PORT, USART_RX_GPIO_PIN,
                 SYS_GPIO_MODE_AF, SYS_GPIO_OTYPE_PP, SYS_GPIO_SPEED_MID, SYS_GPIO_PUPD_PU);

    sys_gpio_af_set(GPIOA, USART_TX_GPIO_PIN, USART_TX_GPIO_AF);
    sys_gpio_af_set(GPIOA, USART_RX_GPIO_PIN, USART_RX_GPIO_AF);

    temp = (sclk * 1000000 + baudrate / 2) / baudrate;

    USART_UX->BRR = temp;
    USART_UX->CR1 = 0;
    USART_UX->CR1 |= 0 << 28;
    USART_UX->CR1 |= 0 << 12;
    USART_UX->CR1 |= 0 << 15;
    USART_UX->CR1 |= 1 << 3;
#if USART_EN_RX

    USART_UX->CR1 |= 1 << 2;
    USART_UX->CR1 |= 1 << 5;
    sys_nvic_init(6, 0, USART_UX_IRQn, 4);
#endif
    USART_UX->CR1 |= 1 << 0;
}
