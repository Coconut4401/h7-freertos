/**
 * @file app_mouse.c
 * @brief 解析 CH9350 鼠标数据并维护指针位置和按键状态。
 * @details 这是 app_mouse 模块的实现文件（App/app_mouse.c）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#include "app_mouse.h"

#include <stddef.h>

#include "task.h"

/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define APP_MOUSE_BUTTON_LEFT     0x01U
#define APP_MOUSE_BUTTON_RIGHT    0x02U
#define APP_MOUSE_BUTTON_MASK     (APP_MOUSE_BUTTON_LEFT | APP_MOUSE_BUTTON_RIGHT)
#define APP_MOUSE_BUTTON_CONFIRM_MS 25U

/** @brief 模块数据类型：描述本模块维护的状态、配置或数据快照。 */
typedef struct
{
    QueueHandle_t event_queue;
    int32_t x;
    int32_t y;
    int16_t low_remainder_x;
    int16_t low_remainder_y;
    uint8_t raw_buttons;
    uint8_t accepted_buttons;
    uint8_t pending_press_buttons;
    uint8_t pending_move;
    uint8_t pending_move_buttons;
    TickType_t left_press_tick;
    TickType_t right_press_tick;
} app_mouse_state_t;

static app_mouse_state_t g_mouse;

/**
 * @brief app_mouse_scale_delta：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param delta 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param remainder 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
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

/**
 * @brief app_mouse_limit_position：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
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

/**
 * @brief app_mouse_send：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param type 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param wheel 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param buttons 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
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

/* A button must survive a later report, or remain held briefly while idle. */
static void app_mouse_confirm_press(uint8_t button)
{
    if ((g_mouse.pending_press_buttons & button) == 0U)
    {
        return;
    }

    app_mouse_flush();
    if (button == APP_MOUSE_BUTTON_LEFT)
    {
        app_mouse_send(APP_INPUT_EVENT_DOWN, 0, g_mouse.raw_buttons);
    }
    else
    {
        app_mouse_send(APP_INPUT_EVENT_BACK, 0, g_mouse.raw_buttons);
    }
    g_mouse.pending_press_buttons &= (uint8_t)~button;
    g_mouse.accepted_buttons |= button;
}

/**
 * @brief app_mouse_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param event_queue 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_mouse_init(QueueHandle_t event_queue)
{
    g_mouse.event_queue = event_queue;
    g_mouse.x = (int32_t)(APP_LCD_WIDTH / 2U);
    g_mouse.y = (int32_t)(APP_LCD_HEIGHT / 2U);
    g_mouse.low_remainder_x = 0;
    g_mouse.low_remainder_y = 0;
    g_mouse.raw_buttons = 0U;
    g_mouse.accepted_buttons = 0U;
    g_mouse.pending_press_buttons = 0U;
    g_mouse.pending_move = 0U;
    g_mouse.pending_move_buttons = 0U;
    g_mouse.left_press_tick = 0U;
    g_mouse.right_press_tick = 0U;
}

/**
 * @brief app_mouse_set_position：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_mouse_set_position(uint16_t x, uint16_t y)
{
    g_mouse.x = (int32_t)x;
    g_mouse.y = (int32_t)y;
    app_mouse_limit_position();
}

/**
 * @brief app_mouse_process_report：解析并处理当前事件或数据，根据结果推进模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param report 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_mouse_process_report(const ch9350_mouse_report_t *report)
{
    uint8_t buttons;
    uint8_t changed_buttons;
    int16_t delta_x;
    int16_t delta_y;
    TickType_t now;

    if (report == NULL || g_mouse.event_queue == NULL)
    {
        return;
    }

    buttons = report->buttons & APP_MOUSE_BUTTON_MASK;
    now = xTaskGetTickCount();
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
        g_mouse.pending_move_buttons = buttons;
    }

    changed_buttons = (uint8_t)(buttons ^ g_mouse.raw_buttons);
    if ((changed_buttons & APP_MOUSE_BUTTON_LEFT) != 0U)
    {
        if ((buttons & APP_MOUSE_BUTTON_LEFT) != 0U)
        {
            g_mouse.pending_press_buttons |= APP_MOUSE_BUTTON_LEFT;
            g_mouse.left_press_tick = now;
        }
        else
        {
            g_mouse.pending_press_buttons &=
                (uint8_t)~APP_MOUSE_BUTTON_LEFT;
            if ((g_mouse.accepted_buttons & APP_MOUSE_BUTTON_LEFT) != 0U)
            {
                app_mouse_flush();
                app_mouse_send(APP_INPUT_EVENT_UP, 0, buttons);
                g_mouse.accepted_buttons &=
                    (uint8_t)~APP_MOUSE_BUTTON_LEFT;
            }
        }
    }

    if ((changed_buttons & APP_MOUSE_BUTTON_RIGHT) != 0U)
    {
        if ((buttons & APP_MOUSE_BUTTON_RIGHT) != 0U)
        {
            g_mouse.pending_press_buttons |= APP_MOUSE_BUTTON_RIGHT;
            g_mouse.right_press_tick = now;
        }
        else
        {
            g_mouse.pending_press_buttons &=
                (uint8_t)~APP_MOUSE_BUTTON_RIGHT;
            g_mouse.accepted_buttons &= (uint8_t)~APP_MOUSE_BUTTON_RIGHT;
        }
    }

    g_mouse.raw_buttons = buttons;

    if ((g_mouse.pending_press_buttons & APP_MOUSE_BUTTON_LEFT) != 0U &&
        (buttons & APP_MOUSE_BUTTON_LEFT) != 0U &&
        (changed_buttons & APP_MOUSE_BUTTON_LEFT) == 0U)
    {
        app_mouse_confirm_press(APP_MOUSE_BUTTON_LEFT);
    }
    if ((g_mouse.pending_press_buttons & APP_MOUSE_BUTTON_RIGHT) != 0U &&
        (buttons & APP_MOUSE_BUTTON_RIGHT) != 0U &&
        (changed_buttons & APP_MOUSE_BUTTON_RIGHT) == 0U)
    {
        app_mouse_confirm_press(APP_MOUSE_BUTTON_RIGHT);
    }

    if (report->wheel != 0)
    {
        app_mouse_flush();
        app_mouse_send(APP_INPUT_EVENT_SCROLL,
                       report->wheel, buttons);
    }
}

void app_mouse_rebaseline(const ch9350_mouse_report_t *report)
{
    uint8_t buttons;

    if (report == NULL || g_mouse.event_queue == NULL)
    {
        return;
    }

    buttons = report->buttons & APP_MOUSE_BUTTON_MASK;
    g_mouse.low_remainder_x = 0;
    g_mouse.low_remainder_y = 0;
    g_mouse.pending_press_buttons = 0U;
    g_mouse.pending_move = 0U;
    g_mouse.pending_move_buttons = 0U;
    g_mouse.left_press_tick = 0U;
    g_mouse.right_press_tick = 0U;
    if ((g_mouse.accepted_buttons & APP_MOUSE_BUTTON_LEFT) != 0U)
    {
        app_mouse_send(APP_INPUT_EVENT_UP, 0, 0U);
    }
    g_mouse.raw_buttons = buttons;
    g_mouse.accepted_buttons = 0U;
}

/**
 * @brief app_mouse_disconnect：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_mouse_disconnect(void)
{
    g_mouse.pending_move = 0U;
    g_mouse.pending_move_buttons = 0U;
    if (g_mouse.accepted_buttons & APP_MOUSE_BUTTON_LEFT)
    {
        app_mouse_send(APP_INPUT_EVENT_UP, 0, 0U);
    }
    g_mouse.low_remainder_x = 0;
    g_mouse.low_remainder_y = 0;
    g_mouse.raw_buttons = 0U;
    g_mouse.accepted_buttons = 0U;
    g_mouse.pending_press_buttons = 0U;
    g_mouse.left_press_tick = 0U;
    g_mouse.right_press_tick = 0U;
}

void app_mouse_poll(void)
{
    TickType_t now;

    now = xTaskGetTickCount();
    if ((g_mouse.pending_press_buttons & APP_MOUSE_BUTTON_LEFT) != 0U &&
        (g_mouse.raw_buttons & APP_MOUSE_BUTTON_LEFT) != 0U &&
        (TickType_t)(now - g_mouse.left_press_tick) >=
        pdMS_TO_TICKS(APP_MOUSE_BUTTON_CONFIRM_MS))
    {
        app_mouse_confirm_press(APP_MOUSE_BUTTON_LEFT);
    }
    if ((g_mouse.pending_press_buttons & APP_MOUSE_BUTTON_RIGHT) != 0U &&
        (g_mouse.raw_buttons & APP_MOUSE_BUTTON_RIGHT) != 0U &&
        (TickType_t)(now - g_mouse.right_press_tick) >=
        pdMS_TO_TICKS(APP_MOUSE_BUTTON_CONFIRM_MS))
    {
        app_mouse_confirm_press(APP_MOUSE_BUTTON_RIGHT);
    }
}

/**
 * @brief app_mouse_flush：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_mouse_flush(void)
{
    if (g_mouse.pending_move)
    {
        g_mouse.pending_move = 0U;
        app_mouse_send(APP_INPUT_EVENT_MOVE, 0,
                       g_mouse.pending_move_buttons);
    }
}
