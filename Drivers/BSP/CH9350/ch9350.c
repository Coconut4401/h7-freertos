#include "ch9350.h"

#include <stddef.h>

#include "./SYSTEM/usart/usart.h"

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

typedef struct
{
    uint8_t frame[CH9350_MAX_FRAME_SIZE];
    uint8_t length;
    uint8_t expected_length;
    ch9350_stats_t stats;
} ch9350_parser_t;

static ch9350_parser_t g_ch9350;

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

static uint8_t ch9350_toggle_mouse_connected(void)
{
    if (!g_ch9350.stats.connection_known)
    {
        return ch9350_set_mouse_connected(1U);
    }

    return ch9350_set_mouse_connected(
        g_ch9350.stats.mouse_connected ? 0U : 1U);
}

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
        /*
         * In state 2, opcode 0x80 reports which report ID changed.  It is
         * not a bitmap of all devices currently connected.  The CH9350
         * sends report ID 0x02 for both mouse insertion and removal.
         */
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

void ch9350_get_stats(ch9350_stats_t *stats)
{
    if (stats != NULL)
    {
        *stats = g_ch9350.stats;
        stats->uart_dropped_count = usart_rx_get_dropped_count();
    }
}
