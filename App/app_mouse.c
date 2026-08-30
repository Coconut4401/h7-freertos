#include "app_mouse.h"

#include <stddef.h>

#include "task.h"

#define APP_MOUSE_BUTTON_LEFT     0x01U
#define APP_MOUSE_BUTTON_RIGHT    0x02U

typedef struct
{
    QueueHandle_t event_queue;
    int32_t x;
    int32_t y;
    int16_t low_remainder_x;
    int16_t low_remainder_y;
    uint8_t previous_buttons;
    uint8_t pending_move;
    uint8_t pending_buttons;
} app_mouse_state_t;

static app_mouse_state_t g_mouse;

static int16_t app_mouse_scale_delta(int8_t delta, int16_t *remainder)
{
    uint8_t sensitivity;
    int16_t accumulated;
    int16_t scaled;

    sensitivity = app_input_get_cursor_sensitivity();
    if (sensitivity == APP_INPUT_SENSITIVITY_HIGH)
    {
        *remainder = 0;
        return (int16_t)delta * 2;
    }
    if (sensitivity == APP_INPUT_SENSITIVITY_LOW)
    {
        accumulated = (int16_t)(*remainder + (int16_t)delta);
        scaled = (int16_t)(accumulated / 2);
        *remainder = (int16_t)(accumulated - scaled * 2);
        return scaled;
    }

    *remainder = 0;
    return (int16_t)delta;
}

static void app_mouse_limit_position(void)
{
    if (g_mouse.x < 0)
    {
        g_mouse.x = 0;
    }
    else if (g_mouse.x >= (int32_t)APP_LCD_WIDTH)
    {
        g_mouse.x = (int32_t)APP_LCD_WIDTH - 1;
    }

    if (g_mouse.y < 0)
    {
        g_mouse.y = 0;
    }
    else if (g_mouse.y >= (int32_t)APP_LCD_HEIGHT)
    {
        g_mouse.y = (int32_t)APP_LCD_HEIGHT - 1;
    }
}

static void app_mouse_send(app_input_event_type_t type,
                           int8_t wheel,
                           uint8_t buttons)
{
    app_input_event_t event;

    event.type = type;
    event.source = APP_INPUT_SOURCE_MOUSE;
    event.x = (uint16_t)g_mouse.x;
    event.y = (uint16_t)g_mouse.y;
    event.wheel = wheel;
    event.buttons = buttons;
    event.tick = (uint32_t)xTaskGetTickCount();
    app_input_post_event(g_mouse.event_queue, &event);
}

void app_mouse_init(QueueHandle_t event_queue)
{
    g_mouse.event_queue = event_queue;
    g_mouse.x = (int32_t)(APP_LCD_WIDTH / 2U);
    g_mouse.y = (int32_t)(APP_LCD_HEIGHT / 2U);
    g_mouse.low_remainder_x = 0;
    g_mouse.low_remainder_y = 0;
    g_mouse.previous_buttons = 0U;
    g_mouse.pending_move = 0U;
    g_mouse.pending_buttons = 0U;
}

void app_mouse_set_position(uint16_t x, uint16_t y)
{
    g_mouse.x = (int32_t)x;
    g_mouse.y = (int32_t)y;
    app_mouse_limit_position();
}

void app_mouse_process_report(const ch9350_mouse_report_t *report)
{
    uint8_t changed_buttons;
    int16_t delta_x;
    int16_t delta_y;

    if (report == NULL || g_mouse.event_queue == NULL)
    {
        return;
    }

    delta_x = app_mouse_scale_delta(report->delta_x,
                                    &g_mouse.low_remainder_x);
    delta_y = app_mouse_scale_delta(report->delta_y,
                                    &g_mouse.low_remainder_y);
    if (delta_x != 0 || delta_y != 0)
    {
        g_mouse.x += delta_x;
        g_mouse.y += delta_y;
        app_mouse_limit_position();
        g_mouse.pending_move = 1U;
        g_mouse.pending_buttons = report->buttons;
    }

    changed_buttons = (uint8_t)(report->buttons ^ g_mouse.previous_buttons);
    if (changed_buttons & APP_MOUSE_BUTTON_LEFT)
    {
        app_mouse_flush();
        app_mouse_send((report->buttons & APP_MOUSE_BUTTON_LEFT) ?
                       APP_INPUT_EVENT_DOWN : APP_INPUT_EVENT_UP,
                       0, report->buttons);
    }

    if ((changed_buttons & APP_MOUSE_BUTTON_RIGHT) &&
        (report->buttons & APP_MOUSE_BUTTON_RIGHT))
    {
        app_mouse_flush();
        app_mouse_send(APP_INPUT_EVENT_BACK, 0, report->buttons);
    }

    if (report->wheel != 0)
    {
        app_mouse_flush();
        app_mouse_send(APP_INPUT_EVENT_SCROLL,
                       report->wheel, report->buttons);
    }

    g_mouse.previous_buttons = report->buttons;
}

void app_mouse_disconnect(void)
{
    app_mouse_flush();
    if (g_mouse.previous_buttons & APP_MOUSE_BUTTON_LEFT)
    {
        app_mouse_send(APP_INPUT_EVENT_UP, 0, 0U);
    }
    g_mouse.low_remainder_x = 0;
    g_mouse.low_remainder_y = 0;
    g_mouse.previous_buttons = 0U;
    g_mouse.pending_move = 0U;
    g_mouse.pending_buttons = 0U;
}

void app_mouse_flush(void)
{
    if (g_mouse.pending_move)
    {
        g_mouse.pending_move = 0U;
        app_mouse_send(APP_INPUT_EVENT_MOVE, 0,
                       g_mouse.pending_buttons);
    }
}
