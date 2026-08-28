#include "app_input.h"

#include "./BSP/TOUCH/touch.h"
#include "task.h"

static volatile uint32_t g_sent_count;
static volatile uint32_t g_dropped_count;
static volatile uint8_t g_cursor_sensitivity = APP_INPUT_SENSITIVITY_NORMAL;

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

static void app_input_send(QueueHandle_t queue, const app_input_event_t *event)
{
    if (xQueueSend(queue, event, 0) == pdPASS)
    {
        g_sent_count++;
    }
    else
    {
        g_dropped_count++;
    }
}

void app_input_get_stats(app_input_stats_t *stats)
{
    if (stats != NULL)
    {
        stats->sent_count = g_sent_count;
        stats->dropped_count = g_dropped_count;
    }
}

void app_input_set_cursor_sensitivity(uint8_t sensitivity)
{
    if (sensitivity >= APP_INPUT_SENSITIVITY_LOW &&
        sensitivity <= APP_INPUT_SENSITIVITY_HIGH)
    {
        g_cursor_sensitivity = sensitivity;
    }
}

uint8_t app_input_get_cursor_sensitivity(void)
{
    return g_cursor_sensitivity;
}

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

    context = (const app_input_task_context_t *)argument;
    last_x = 0U;
    last_y = 0U;
    was_pressed = 0U;
    last_wake = xTaskGetTickCount();

    while (1)
    {
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
                    event.x = tp_dev.x[0];
                    event.y = tp_dev.y[0];
                    event.tick = (uint32_t)xTaskGetTickCount();
                    app_input_send(context->event_queue, &event);

                    last_x = event.x;
                    last_y = event.y;
                }

                was_pressed = 1U;
            }
            else if (was_pressed)
            {
                event.type = APP_INPUT_EVENT_UP;
                event.x = last_x;
                event.y = last_y;
                event.tick = (uint32_t)xTaskGetTickCount();
                app_input_send(context->event_queue, &event);
                was_pressed = 0U;
            }
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(5));
    }
}
