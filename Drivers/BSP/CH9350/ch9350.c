/**
 * @file ch9350.c
 * @brief 配置并读取 CH9350 USB 主机芯片，解析其串口 HID 数据。
 * @details 这是 ch9350 模块的实现文件（Drivers/BSP/CH9350/ch9350.c）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#include "ch9350.h"

#include <stddef.h>

#include "./SYSTEM/usart/usart.h"

/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define CH9350_HEADER_0             0x57U
#define CH9350_HEADER_1             0xABU
#define CH9350_OPCODE_KEYBOARD      0x01U
#define CH9350_OPCODE_MOUSE_REL     0x02U
#define CH9350_OPCODE_MOUSE_ABS     0x04U
#define CH9350_OPCODE_STATE         0x80U
#define CH9350_OPCODE_DISCONNECT    0x86U
#define CH9350_OPCODE_VERSION       0x87U
#define CH9350_OPCODE_TIMEOUT       0x89U
#define CH9350_MAX_FRAME_SIZE       11U
#define CH9350_STATE_REPORT_MASK    0x0FU
#define CH9350_REPORT_ID_MOUSE_REL  0x02U

/** @brief 模块数据类型：描述本模块维护的状态、配置或数据快照。 */
typedef struct
{
    uint8_t frame[CH9350_MAX_FRAME_SIZE];
    uint8_t length;
    uint8_t expected_length;
    ch9350_stats_t stats;
} ch9350_parser_t;

static ch9350_parser_t g_ch9350;

/**
 * @brief ch9350_set_mouse_connected：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param connected 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint8_t ch9350_set_mouse_connected(uint8_t connected)
{
    connected = connected ? 1U : 0U;
    if (g_ch9350.stats.connection_known &&
        g_ch9350.stats.mouse_connected == connected)
    {
        return 0U;
    }

    g_ch9350.stats.connection_known = 1U;
    g_ch9350.stats.mouse_connected = connected;
    if (connected)
    {
        g_ch9350.stats.connect_event_count++;
    }
    else
    {
        g_ch9350.stats.disconnect_event_count++;
    }
    return 1U;
}

/**
 * @brief ch9350_toggle_mouse_connected：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint8_t ch9350_toggle_mouse_connected(void)
{
    if (!g_ch9350.stats.connection_known)
    {
        return ch9350_set_mouse_connected(1U);
    }

    return ch9350_set_mouse_connected(
        g_ch9350.stats.mouse_connected ? 0U : 1U);
}

/**
 * @brief ch9350_send_state_response：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
static void ch9350_send_state_response(void)
{
    static const uint8_t response[11] =
    {
        0x57U, 0xABU, 0x12U,
        0x00U, 0x00U, 0x00U, 0x00U,
        0xFFU, 0xFFU, 0x00U, 0x20U
    };

    usart_tx_write(response, (uint16_t)sizeof(response));
}

/**
 * @brief ch9350_frame_length：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param opcode 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint8_t ch9350_frame_length(uint8_t opcode)
{
    switch (opcode)
    {
        case CH9350_OPCODE_KEYBOARD:
            return 11U;

        case CH9350_OPCODE_MOUSE_REL:
            return 7U;

        case CH9350_OPCODE_MOUSE_ABS:
            return 10U;

        case CH9350_OPCODE_STATE:
            return 4U;

        case CH9350_OPCODE_DISCONNECT:
        case CH9350_OPCODE_VERSION:
        case CH9350_OPCODE_TIMEOUT:
            return 3U;

        default:
            return 0U;
    }
}

/**
 * @brief ch9350_restart_sync：使用最新数据更新缓存、硬件输出或界面显示状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param byte 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void ch9350_restart_sync(uint8_t byte)
{
    g_ch9350.length = 0U;
    g_ch9350.expected_length = 0U;
    if (byte == CH9350_HEADER_0)
    {
        g_ch9350.frame[0] = byte;
        g_ch9350.length = 1U;
    }
}

/**
 * @brief ch9350_parse_byte：解析并处理当前事件或数据，根据结果推进模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param byte 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param event 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint8_t ch9350_parse_byte(uint8_t byte, ch9350_event_t *event)
{
    uint8_t expected;
    uint8_t report_id;

    if (g_ch9350.length == 0U)
    {
        if (byte == CH9350_HEADER_0)
        {
            g_ch9350.frame[0] = byte;
            g_ch9350.length = 1U;
        }
        return 0U;
    }

    if (g_ch9350.length == 1U)
    {
        if (byte == CH9350_HEADER_1)
        {
            g_ch9350.frame[1] = byte;
            g_ch9350.length = 2U;
        }
        else
        {
            g_ch9350.stats.sync_error_count++;
            ch9350_restart_sync(byte);
        }
        return 0U;
    }

    if (g_ch9350.length == 2U)
    {
        expected = ch9350_frame_length(byte);
        if (expected == 0U)
        {
            g_ch9350.stats.sync_error_count++;
            ch9350_restart_sync(byte);
            return 0U;
        }
        g_ch9350.frame[2] = byte;
        g_ch9350.length = 3U;
        g_ch9350.expected_length = expected;
    }
    else
    {
        g_ch9350.frame[g_ch9350.length] = byte;
        g_ch9350.length++;
    }

    if (g_ch9350.length < g_ch9350.expected_length)
    {
        return 0U;
    }

    if (g_ch9350.frame[2] == CH9350_OPCODE_MOUSE_REL)
    {
        event->type = CH9350_EVENT_MOUSE_REPORT;
        event->mouse_report.buttons = g_ch9350.frame[3];
        event->mouse_report.delta_x = (int8_t)g_ch9350.frame[4];
        event->mouse_report.delta_y = (int8_t)g_ch9350.frame[5];
        event->mouse_report.wheel = (int8_t)g_ch9350.frame[6];
        event->connection_changed = ch9350_set_mouse_connected(1U);
        event->mouse_connected = 1U;
        g_ch9350.stats.mouse_report_count++;
        g_ch9350.length = 0U;
        g_ch9350.expected_length = 0U;
        return 1U;
    }

    if (g_ch9350.frame[2] == CH9350_OPCODE_STATE)
    {
        g_ch9350.stats.state_frame_count++;
        g_ch9350.stats.last_state_value = g_ch9350.frame[3];

        report_id = g_ch9350.frame[3] & CH9350_STATE_REPORT_MASK;
        g_ch9350.length = 0U;
        g_ch9350.expected_length = 0U;
        ch9350_send_state_response();

        if ((report_id & CH9350_REPORT_ID_MOUSE_REL) == 0U)
        {
            return 0U;
        }

        event->type = CH9350_EVENT_CONNECTION;
        event->connection_changed = ch9350_toggle_mouse_connected();
        event->mouse_connected = g_ch9350.stats.mouse_connected;
        return event->connection_changed;
    }

    if (g_ch9350.frame[2] == CH9350_OPCODE_DISCONNECT)
    {
        g_ch9350.stats.state_frame_count++;
        g_ch9350.length = 0U;
        g_ch9350.expected_length = 0U;
        event->type = CH9350_EVENT_CONNECTION;
        event->connection_changed = ch9350_set_mouse_connected(0U);
        event->mouse_connected = 0U;
        return event->connection_changed;
    }

    g_ch9350.stats.discarded_frame_count++;
    g_ch9350.length = 0U;
    g_ch9350.expected_length = 0U;
    return 0U;
}

/**
 * @brief ch9350_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void ch9350_init(void)
{
    uint8_t index;

    for (index = 0U; index < CH9350_MAX_FRAME_SIZE; index++)
    {
        g_ch9350.frame[index] = 0U;
    }
    g_ch9350.length = 0U;
    g_ch9350.expected_length = 0U;
    g_ch9350.stats.mouse_report_count = 0U;
    g_ch9350.stats.state_frame_count = 0U;
    g_ch9350.stats.connect_event_count = 0U;
    g_ch9350.stats.disconnect_event_count = 0U;
    g_ch9350.stats.discarded_frame_count = 0U;
    g_ch9350.stats.sync_error_count = 0U;
    g_ch9350.stats.uart_dropped_count = 0U;
    g_ch9350.stats.connection_known = 0U;
    g_ch9350.stats.mouse_connected = 0U;
    g_ch9350.stats.last_state_value = 0U;
    usart_rx_reset();
    ch9350_send_state_response();
}

/**
 * @brief ch9350_read_event：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param event 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t ch9350_read_event(ch9350_event_t *event)
{
    uint8_t byte;

    if (event == NULL)
    {
        return 0U;
    }

    event->connection_changed = 0U;
    while (usart_rx_read_byte(&byte))
    {
        if (ch9350_parse_byte(byte, event))
        {
            return 1U;
        }
    }
    return 0U;
}

/**
 * @brief ch9350_get_stats：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param stats 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void ch9350_get_stats(ch9350_stats_t *stats)
{
    if (stats != NULL)
    {
        *stats = g_ch9350.stats;
        stats->uart_dropped_count = usart_rx_get_dropped_count();
    }
}
