/**
 * @file app_input.c
 * @brief 把触摸和鼠标原始输入转换为供应用消费的统一输入事件。
 * @details 这是 app_input 模块的实现文件（App/app_input.c）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#include "app_input.h"

#include "./BSP/TOUCH/touch.h"
#include "./BSP/CH9350/ch9350.h"
#include "app_mouse.h"
#include "app_health.h"
#include "task.h"

static volatile uint32_t g_sent_count;
static volatile uint32_t g_raw_event_count;
static volatile uint32_t g_raw_move_count;
static volatile uint32_t g_raw_scroll_count;
static volatile uint32_t g_consumed_count;
static volatile uint32_t g_producer_merged_count;
static volatile uint32_t g_consumer_merged_count;
static volatile uint32_t g_dropped_count;
static volatile uint32_t g_throttled_move_count;
static volatile uint32_t g_throttled_scroll_count;
static volatile uint32_t g_expired_move_count;
static volatile uint32_t g_dropped_move_count;
static volatile uint32_t g_critical_drop_count;
static volatile uint32_t g_latency_sample_count;
static volatile uint32_t g_latency_total_ms;
static volatile uint16_t g_latency_current_ms;
static volatile uint16_t g_latency_max_ms;
static volatile uint16_t g_queue_high_water;
static volatile uint16_t g_queue_depth;
static volatile app_input_scheduler_mode_t g_scheduler_mode;
static app_input_event_t g_pending_move;
static app_input_event_t g_pending_scroll;
static uint8_t g_pending_move_valid;
static uint8_t g_pending_scroll_valid;
static TickType_t g_last_move_sent;
static TickType_t g_last_scroll_sent;
static TickType_t g_test_started;
static volatile app_input_test_mode_t g_test_mode;
static uint8_t g_test_complete;
static uint8_t g_test_passed;
static volatile uint8_t g_cursor_sensitivity = APP_INPUT_SENSITIVITY_NORMAL;

/**
 * @brief app_input_movement_threshold：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint16_t app_input_movement_threshold(void)
{
    if (g_cursor_sensitivity == APP_INPUT_SENSITIVITY_LOW)
    {
        return 6U;
    }
    if (g_cursor_sensitivity == APP_INPUT_SENSITIVITY_HIGH)
    {
        return 1U;
    }
    return 3U;
}

static uint16_t app_input_move_interval_ms(UBaseType_t depth)
{
    if (depth >= (APP_INPUT_QUEUE_LENGTH - APP_INPUT_QUEUE_RESERVED))
    {
        return 0U;
    }
    if (depth >= 16U)
    {
        return 33U;
    }
    if (depth >= 8U)
    {
        return 16U;
    }
    return 10U;
}

static app_input_scheduler_mode_t app_input_mode(UBaseType_t depth)
{
    if (depth >= (APP_INPUT_QUEUE_LENGTH - APP_INPUT_QUEUE_RESERVED))
    {
        return APP_INPUT_MODE_PROTECT;
    }
    if (depth >= 16U)
    {
        return APP_INPUT_MODE_PRESSURE;
    }
    if (depth >= 8U)
    {
        return APP_INPUT_MODE_BUSY;
    }
    return APP_INPUT_MODE_NORMAL;
}

static BaseType_t app_input_send_direct(QueueHandle_t queue,
                                        const app_input_event_t *event,
                                        uint8_t critical)
{
    app_input_event_t copy;
    UBaseType_t depth;

    copy = *event;
    copy.enqueue_tick = (uint32_t)xTaskGetTickCount();
    if (xQueueSend(queue, &copy, critical ? pdMS_TO_TICKS(20U) : 0U) != pdPASS)
    {
        g_dropped_count++;
        if (critical)
        {
            g_critical_drop_count++;
        }
        else
        {
            g_dropped_move_count++;
        }
        return pdFAIL;
    }
    g_sent_count++;
    depth = uxQueueMessagesWaiting(queue);
    g_queue_depth = (uint16_t)depth;
    if (depth > g_queue_high_water)
    {
        g_queue_high_water = (uint16_t)depth;
    }
    return pdPASS;
}

static void app_input_flush_pending(QueueHandle_t queue, TickType_t now,
                                    uint8_t force)
{
    UBaseType_t depth;
    uint16_t interval;

    depth = uxQueueMessagesWaiting(queue);
    g_queue_depth = (uint16_t)depth;
    g_scheduler_mode = app_input_mode(depth);
    interval = app_input_move_interval_ms(depth);
    if (g_pending_move_valid)
    {
        if ((uint32_t)(now - g_pending_move.tick) > pdMS_TO_TICKS(50U))
        {
            g_pending_move_valid = 0U;
            g_expired_move_count++;
            g_dropped_count++;
            g_dropped_move_count++;
        }
        else if (force &&
                 depth >= (APP_INPUT_QUEUE_LENGTH - APP_INPUT_QUEUE_RESERVED))
        {
            g_pending_move_valid = 0U;
            g_dropped_count++;
            g_dropped_move_count++;
        }
        else if (force ||
                 (interval != 0U &&
                  (uint32_t)(now - g_last_move_sent) >= pdMS_TO_TICKS(interval)))
        {
            if (app_input_send_direct(queue, &g_pending_move, 0U) == pdPASS)
            {
                g_last_move_sent = now;
                g_pending_move_valid = 0U;
            }
        }
    }
    if (g_pending_scroll_valid &&
        (uint32_t)(now - g_pending_scroll.tick) > pdMS_TO_TICKS(100U))
    {
        g_pending_scroll_valid = 0U;
        g_dropped_count++;
    }
    else if (g_pending_scroll_valid && force &&
        depth >= (APP_INPUT_QUEUE_LENGTH - APP_INPUT_QUEUE_RESERVED))
    {
        g_pending_scroll_valid = 0U;
        g_dropped_count++;
    }
    else if (g_pending_scroll_valid &&
        depth < (APP_INPUT_QUEUE_LENGTH - APP_INPUT_QUEUE_RESERVED) &&
        (force || (uint32_t)(now - g_last_scroll_sent) >= pdMS_TO_TICKS(40U)))
    {
        if (app_input_send_direct(queue, &g_pending_scroll, 0U) == pdPASS)
        {
            g_last_scroll_sent = now;
            g_pending_scroll_valid = 0U;
        }
    }
}

static void app_input_cancel_mouse_pending(void)
{
    if (g_pending_move_valid &&
        g_pending_move.source == APP_INPUT_SOURCE_MOUSE)
    {
        g_pending_move_valid = 0U;
    }
    if (g_pending_scroll_valid &&
        g_pending_scroll.source == APP_INPUT_SOURCE_MOUSE)
    {
        g_pending_scroll_valid = 0U;
    }
}

/**
 * @brief app_input_post_event：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param queue 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param event 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
BaseType_t app_input_post_event(QueueHandle_t queue,
                                const app_input_event_t *event)
{
    TickType_t now;
    UBaseType_t depth;
    uint16_t interval;
    int16_t wheel_sum;

    if (queue == NULL || event == NULL)
    {
        return pdFAIL;
    }
    g_raw_event_count++;
    now = xTaskGetTickCount();
    if (event->type == APP_INPUT_EVENT_MOVE)
    {
        g_raw_move_count++;
        if (g_pending_move_valid &&
            g_pending_move.source != event->source)
        {
            app_input_flush_pending(queue, now, 1U);
        }
        depth = uxQueueMessagesWaiting(queue);
        interval = app_input_move_interval_ms(depth);
        if (g_pending_move_valid)
        {
            g_producer_merged_count++;
            g_throttled_move_count++;
        }
        else if (interval == 0U ||
                 (uint32_t)(now - g_last_move_sent) < pdMS_TO_TICKS(interval))
        {
            g_throttled_move_count++;
        }
        g_pending_move = *event;
        g_pending_move.tick = (uint32_t)now;
        g_pending_move_valid = 1U;
        return pdPASS;
    }
    if (event->type == APP_INPUT_EVENT_SCROLL)
    {
        g_raw_scroll_count++;
        if (g_pending_scroll_valid &&
            ((g_pending_scroll.wheel > 0) == (event->wheel > 0)))
        {
            wheel_sum = (int16_t)g_pending_scroll.wheel + event->wheel;
            if (wheel_sum > 127) wheel_sum = 127;
            if (wheel_sum < -127) wheel_sum = -127;
            g_pending_scroll.wheel = (int8_t)wheel_sum;
            g_throttled_scroll_count++;
        }
        else
        {
            app_input_flush_pending(queue, now, 0U);
            g_pending_scroll = *event;
            g_pending_scroll.tick = (uint32_t)now;
            g_pending_scroll_valid = 1U;
        }
        return pdPASS;
    }
    app_input_flush_pending(queue, now, 1U);
    return app_input_send_direct(queue, event, 1U);
}

void app_input_scheduler_poll(QueueHandle_t queue, TickType_t now)
{
    static uint16_t flood_phase;
    app_input_event_t event;
    uint32_t duration_ms;
    uint8_t index;

    if (queue != NULL)
    {
        if (g_test_mode == APP_INPUT_TEST_FLOOD && !g_test_complete)
        {
            for (index = 0U; index < 6U; index++)
            {
                flood_phase = (uint16_t)((flood_phase + 7U) % 1400U);
                event.type = APP_INPUT_EVENT_MOVE;
                event.source = APP_INPUT_SOURCE_TEST;
                event.x = (uint16_t)(50U + flood_phase % 700U);
                event.y = (uint16_t)(70U + (flood_phase * 3U) % 340U);
                event.wheel = 0;
                event.buttons = 0U;
                event.tick = (uint32_t)now;
                event.enqueue_tick = 0U;
                (void)app_input_post_event(queue, &event);
            }
        }
        app_input_flush_pending(queue, now, 0U);
        if (g_test_mode != APP_INPUT_TEST_NONE && !g_test_complete)
        {
            duration_ms = (g_test_mode == APP_INPUT_TEST_CAPTURE) ?
                60000U : 10000U;
            if ((uint32_t)(now - g_test_started) >= pdMS_TO_TICKS(duration_ms))
            {
                g_test_complete = 1U;
                g_test_passed = (g_critical_drop_count == 0U &&
                                 g_latency_max_ms <= 100U &&
                                 g_queue_high_water <= 28U) ? 1U : 0U;
            }
        }
    }
}

BaseType_t app_input_receive_event(QueueHandle_t queue,
                                   app_input_event_t *event,
                                   TickType_t timeout)
{
    app_input_event_t next;
    TickType_t now;

    if (queue == NULL || event == NULL ||
        xQueueReceive(queue, event, timeout) != pdPASS)
    {
        return pdFAIL;
    }
    g_consumed_count++;
    if (event->type == APP_INPUT_EVENT_MOVE)
    {
        while (xQueuePeek(queue, &next, 0U) == pdPASS &&
               next.type == APP_INPUT_EVENT_MOVE &&
               next.source == event->source)
        {
            (void)xQueueReceive(queue, event, 0U);
            g_consumer_merged_count++;
            g_consumed_count++;
        }
    }
    now = xTaskGetTickCount();
    g_latency_current_ms = (event->enqueue_tick == 0U) ? 0U :
        (uint16_t)((now - (TickType_t)event->enqueue_tick) *
                   1000U / configTICK_RATE_HZ);
    g_latency_sample_count++;
    g_latency_total_ms += g_latency_current_ms;
    if (g_latency_current_ms > g_latency_max_ms)
    {
        g_latency_max_ms = g_latency_current_ms;
    }
    g_queue_depth = (uint16_t)uxQueueMessagesWaiting(queue);
    return pdPASS;
}

BaseType_t app_input_test_start(app_input_test_mode_t mode)
{
    if (mode == APP_INPUT_TEST_NONE)
    {
        return pdFAIL;
    }
    app_input_reset_stats();
    g_test_mode = mode;
    g_test_started = xTaskGetTickCount();
    g_test_complete = 0U;
    g_test_passed = 0U;
    return pdPASS;
}

void app_input_reset_stats(void)
{
    taskENTER_CRITICAL();
    g_sent_count = 0U; g_raw_event_count = 0U; g_raw_move_count = 0U;
    g_raw_scroll_count = 0U; g_consumed_count = 0U;
    g_producer_merged_count = 0U; g_consumer_merged_count = 0U;
    g_dropped_count = 0U; g_throttled_move_count = 0U;
    g_throttled_scroll_count = 0U; g_expired_move_count = 0U;
    g_dropped_move_count = 0U; g_critical_drop_count = 0U;
    g_latency_sample_count = 0U; g_latency_total_ms = 0U;
    g_latency_current_ms = 0U; g_latency_max_ms = 0U;
    g_queue_high_water = 0U; g_scheduler_mode = APP_INPUT_MODE_NORMAL;
    g_queue_depth = 0U;
    g_pending_move_valid = 0U; g_pending_scroll_valid = 0U;
    g_test_mode = APP_INPUT_TEST_NONE; g_test_complete = 0U;
    g_test_passed = 0U; g_test_started = 0U;
    ch9350_reset_stats();
    taskEXIT_CRITICAL();
}

/**
 * @brief app_input_get_stats：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param stats 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_input_get_stats(app_input_stats_t *stats)
{
    if (stats != NULL)
    {
        stats->raw_event_count = g_raw_event_count;
        stats->raw_move_count = g_raw_move_count;
        stats->raw_scroll_count = g_raw_scroll_count;
        stats->sent_count = g_sent_count;
        stats->consumed_count = g_consumed_count;
        stats->producer_merged_count = g_producer_merged_count;
        stats->consumer_merged_count = g_consumer_merged_count;
        stats->dropped_count = g_dropped_count;
        stats->throttled_move_count = g_throttled_move_count;
        stats->throttled_scroll_count = g_throttled_scroll_count;
        stats->expired_move_count = g_expired_move_count;
        stats->dropped_move_count = g_dropped_move_count;
        stats->critical_drop_count = g_critical_drop_count;
        stats->latency_sample_count = g_latency_sample_count;
        stats->latency_average_ms = g_latency_sample_count ?
            (uint32_t)(g_latency_total_ms / g_latency_sample_count) : 0U;
        stats->latency_current_ms = g_latency_current_ms;
        stats->latency_max_ms = g_latency_max_ms;
        stats->queue_depth = g_queue_depth;
        stats->queue_high_water = g_queue_high_water;
        stats->move_interval_ms = app_input_move_interval_ms(g_queue_depth);
        stats->scheduler_mode = (uint8_t)g_scheduler_mode;
        stats->test_mode = (uint8_t)g_test_mode;
        stats->test_complete = g_test_complete;
        stats->test_passed = g_test_passed;
        stats->test_remaining_seconds = 0U;
        if (g_test_mode != APP_INPUT_TEST_NONE && !g_test_complete)
        {
            uint32_t elapsed_ms;
            uint32_t duration_ms;
            elapsed_ms = (uint32_t)(xTaskGetTickCount() - g_test_started) *
                         1000U / configTICK_RATE_HZ;
            duration_ms = (g_test_mode == APP_INPUT_TEST_CAPTURE) ?
                60000U : 10000U;
            if (elapsed_ms < duration_ms)
            {
                stats->test_remaining_seconds =
                    (uint16_t)((duration_ms - elapsed_ms + 999U) / 1000U);
            }
        }
        stats->test_queue_peak = g_queue_high_water;
        stats->test_latency_max_ms = g_latency_max_ms;
        stats->test_raw_count = g_raw_event_count;
        stats->test_merged_count = g_producer_merged_count +
                                   g_consumer_merged_count;
        stats->test_dropped_count = g_dropped_count;
        stats->test_critical_drop_count = g_critical_drop_count;
    }
}

/**
 * @brief app_input_set_cursor_sensitivity：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param sensitivity 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_input_set_cursor_sensitivity(uint8_t sensitivity)
{
    if (sensitivity >= APP_INPUT_SENSITIVITY_LOW &&
        sensitivity <= APP_INPUT_SENSITIVITY_HIGH)
    {
        g_cursor_sensitivity = sensitivity;
    }
}

/**
 * @brief app_input_get_cursor_sensitivity：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t app_input_get_cursor_sensitivity(void)
{
    return g_cursor_sensitivity;
}

/**
 * @brief AppInputTask：作为 FreeRTOS 任务入口，循环处理事件、周期工作和运行状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param argument 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 * @warning 该入口具有特定中断或任务上下文，禁止执行不符合该上下文约束的操作。
 */
void AppInputTask(void *argument)
{
    const app_input_task_context_t *context;
    app_input_event_t event;
    TickType_t last_wake;
    uint16_t last_x;
    uint16_t last_y;
    uint8_t was_pressed;
    uint8_t is_pressed;
    uint8_t valid_pressed;
    uint16_t delta_x;
    uint16_t delta_y;
    uint16_t movement_threshold;
    ch9350_event_t ch9350_event;

    context = (const app_input_task_context_t *)argument;
    last_x = 0U;
    last_y = 0U;
    was_pressed = 0U;
    last_wake = xTaskGetTickCount();
    app_mouse_init(context->event_queue);
    ch9350_init();

    while (1)
    {
        app_health_beat(APP_HEALTH_INPUT);
        if (context->touch_available)
        {
            tp_dev.scan(0);
            is_pressed = (tp_dev.sta & TP_PRES_DOWN) ? 1U : 0U;
            valid_pressed = (is_pressed &&
                             tp_dev.x[0] < APP_LCD_WIDTH &&
                             tp_dev.y[0] < APP_LCD_HEIGHT) ? 1U : 0U;

            if (valid_pressed)
            {
                delta_x = (tp_dev.x[0] >= last_x) ?
                    (uint16_t)(tp_dev.x[0] - last_x) :
                    (uint16_t)(last_x - tp_dev.x[0]);
                delta_y = (tp_dev.y[0] >= last_y) ?
                    (uint16_t)(tp_dev.y[0] - last_y) :
                    (uint16_t)(last_y - tp_dev.y[0]);
                movement_threshold = app_input_movement_threshold();
                if (!was_pressed ||
                    delta_x >= movement_threshold ||
                    delta_y >= movement_threshold)
                {
                    event.type = was_pressed ? APP_INPUT_EVENT_MOVE : APP_INPUT_EVENT_DOWN;
                    event.source = APP_INPUT_SOURCE_TOUCH;
                    event.x = tp_dev.x[0];
                    event.y = tp_dev.y[0];
                    event.wheel = 0;
                    event.buttons = 1U;
                    event.tick = (uint32_t)xTaskGetTickCount();
                    app_input_post_event(context->event_queue, &event);
                    app_mouse_set_position(event.x, event.y);

                    last_x = event.x;
                    last_y = event.y;
                }

                was_pressed = 1U;
            }
            else if (was_pressed)
            {
                event.type = APP_INPUT_EVENT_UP;
                event.source = APP_INPUT_SOURCE_TOUCH;
                event.x = last_x;
                event.y = last_y;
                event.wheel = 0;
                event.buttons = 0U;
                event.tick = (uint32_t)xTaskGetTickCount();
                app_input_post_event(context->event_queue, &event);
                was_pressed = 0U;
            }
        }

        while (ch9350_read_event(&ch9350_event))
        {
            if (ch9350_event.connection_changed)
            {
                if (!ch9350_event.mouse_connected)
                {
                    app_input_cancel_mouse_pending();
                    app_mouse_disconnect();
                }
                event.type = ch9350_event.mouse_connected ?
                    APP_INPUT_EVENT_MOUSE_CONNECTED :
                    APP_INPUT_EVENT_MOUSE_DISCONNECTED;
                event.source = APP_INPUT_SOURCE_MOUSE;
                event.x = 0U;
                event.y = 0U;
                event.wheel = 0;
                event.buttons = 0U;
                event.tick = (uint32_t)xTaskGetTickCount();
                app_input_post_event(context->event_queue, &event);
            }
            if (ch9350_event.type == CH9350_EVENT_MOUSE_REPORT)
            {
                if (ch9350_event.report_rebaseline)
                {
                    app_input_cancel_mouse_pending();
                    app_mouse_rebaseline(&ch9350_event.mouse_report);
                }
                else
                {
                    app_mouse_process_report(&ch9350_event.mouse_report);
                }
            }
        }
        app_mouse_poll();
        app_mouse_flush();
        app_input_scheduler_poll(context->event_queue, xTaskGetTickCount());

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(5));
    }
}
