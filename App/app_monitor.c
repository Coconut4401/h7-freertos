#include "app_monitor.h"

#include <stdio.h>

#include "./BSP/LED/led.h"
#include "app_input.h"
#include "app_settings.h"

static app_monitor_snapshot_t g_monitor_snapshot;

void app_monitor_get_snapshot(app_monitor_snapshot_t *snapshot)
{
    if (snapshot == NULL)
    {
        return;
    }

    taskENTER_CRITICAL();
    *snapshot = g_monitor_snapshot;
    taskEXIT_CRITICAL();
}

void AppMonitorTask(void *argument)
{
    const app_monitor_context_t *context;
    app_input_stats_t input_stats;
    TickType_t last_wake;
    UBaseType_t queue_depth;
    UBaseType_t queue_high_water;
    UBaseType_t input_stack;
    UBaseType_t runtime_stack;
    UBaseType_t monitor_stack;
    app_monitor_snapshot_t snapshot;

    context = (const app_monitor_context_t *)argument;
    queue_high_water = 0U;
    last_wake = xTaskGetTickCount();

    while (1)
    {
        LED1_TOGGLE();
        queue_depth = uxQueueMessagesWaiting(context->event_queue);
        if (queue_depth > queue_high_water)
        {
            queue_high_water = queue_depth;
        }

        app_input_get_stats(&input_stats);
        input_stack = uxTaskGetStackHighWaterMark(context->input_task);
        runtime_stack = uxTaskGetStackHighWaterMark(context->runtime_task);
        monitor_stack = uxTaskGetStackHighWaterMark(NULL);

        snapshot.uptime_seconds = (uint32_t)(xTaskGetTickCount() /
                                             configTICK_RATE_HZ);
        snapshot.free_heap_bytes = (uint32_t)xPortGetFreeHeapSize();
        snapshot.input_event_count = input_stats.sent_count;
        snapshot.dropped_event_count = input_stats.dropped_count;
        snapshot.queue_depth = (uint16_t)queue_depth;
        snapshot.queue_high_water = (uint16_t)queue_high_water;
        snapshot.input_stack_watermark = (uint16_t)input_stack;
        snapshot.runtime_stack_watermark = (uint16_t)runtime_stack;
        snapshot.monitor_stack_watermark = (uint16_t)monitor_stack;

        taskENTER_CRITICAL();
        g_monitor_snapshot = snapshot;
        taskEXIT_CRITICAL();

        if (app_settings_get_serial_output_enabled())
        {
            printf("RTOS tick=%lu heap=%u queue=%u/%u input=%lu drop=%lu stack(I/G/M)=%u/%u/%u\r\n",
                   (unsigned long)xTaskGetTickCount(),
                   (unsigned int)snapshot.free_heap_bytes,
                   (unsigned int)queue_depth,
                   (unsigned int)queue_high_water,
                   (unsigned long)input_stats.sent_count,
                   (unsigned long)input_stats.dropped_count,
                   (unsigned int)input_stack,
                   (unsigned int)runtime_stack,
                   (unsigned int)monitor_stack);
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(500U));
    }
}
