#include "app_monitor.h"

#include <stdio.h>

#include "./BSP/CH9350/ch9350.h"
#include "./BSP/LED/led.h"
#include "app_input.h"
#include "app_settings.h"
#include "app_health.h"
#include "app_storage.h"
#include "app_logs.h"
#include "app_fault.h"

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
    ch9350_stats_t ch9350_stats;
    app_health_snapshot_t health_stats;
    app_storage_stats_t storage_stats;
    app_fault_record_t boot_record;
    TickType_t last_wake;
    UBaseType_t queue_depth;
    UBaseType_t queue_high_water;
    app_monitor_snapshot_t snapshot;
    uint32_t previous_low_stack_mask;
    uint8_t low_heap_logged;

    context = (const app_monitor_context_t *)argument;
    queue_high_water = 0U;
    previous_low_stack_mask = 0U;
    low_heap_logged = 0U;
    last_wake = xTaskGetTickCount();

    while (1)
    {
        app_health_beat(APP_HEALTH_MONITOR);
        LED1_TOGGLE();
        queue_depth = uxQueueMessagesWaiting(context->event_queue);
        if (queue_depth > queue_high_water)
        {
            queue_high_water = queue_depth;
        }

        app_input_get_stats(&input_stats);
        ch9350_get_stats(&ch9350_stats);
        app_health_get_snapshot(&health_stats);
        app_storage_get_stats(&storage_stats);
        app_fault_get_boot_record(&boot_record);

        snapshot.uptime_seconds = (uint32_t)(xTaskGetTickCount() /
                                             configTICK_RATE_HZ);
        snapshot.free_heap_bytes = health_stats.current_heap;
        snapshot.minimum_heap_bytes = health_stats.minimum_heap;
        snapshot.input_event_count = input_stats.sent_count;
        snapshot.dropped_event_count = input_stats.dropped_count;
        snapshot.throttled_move_count = input_stats.throttled_move_count;
        snapshot.critical_drop_count = input_stats.critical_drop_count;
        snapshot.ch9350_mouse_report_count = ch9350_stats.mouse_report_count;
        snapshot.ch9350_state_frame_count = ch9350_stats.state_frame_count;
        snapshot.ch9350_connect_event_count = ch9350_stats.connect_event_count;
        snapshot.ch9350_disconnect_event_count =
            ch9350_stats.disconnect_event_count;
        snapshot.ch9350_discarded_frame_count =
            ch9350_stats.discarded_frame_count;
        snapshot.ch9350_sync_error_count = ch9350_stats.sync_error_count;
        snapshot.ch9350_uart_dropped_count = ch9350_stats.uart_dropped_count;
        snapshot.queue_depth = (uint16_t)queue_depth;
        snapshot.queue_high_water = (uint16_t)queue_high_water;
        snapshot.input_stack_watermark =
            (uint16_t)health_stats.stack_words[APP_HEALTH_INPUT];
        snapshot.runtime_stack_watermark =
            (uint16_t)health_stats.stack_words[APP_HEALTH_RUNTIME];
        snapshot.storage_stack_watermark =
            (uint16_t)health_stats.stack_words[APP_HEALTH_STORAGE];
        snapshot.audio_stack_watermark =
            (uint16_t)health_stats.stack_words[APP_HEALTH_AUDIO];
        snapshot.monitor_stack_watermark =
            (uint16_t)health_stats.stack_words[APP_HEALTH_MONITOR];
        snapshot.health_stack_watermark =
            (uint16_t)health_stats.health_stack_words;
        snapshot.timer_stack_watermark =
            (uint16_t)health_stats.timer_stack_words;
        snapshot.storage_queue_depth = storage_stats.request_queue_depth;
        snapshot.storage_queue_peak =
            (uint16_t)storage_stats.request_queue_peak;
        snapshot.storage_queue_full_count =
            storage_stats.request_queue_full_count;
        snapshot.storage_response_drop_count =
            storage_stats.response_drop_count;
        snapshot.storage_error_count = storage_stats.filesystem_error_count;
        snapshot.healthy_task_mask = health_stats.healthy_mask;
        snapshot.task_timeout_mask = health_stats.timeout_mask;
        snapshot.low_stack_mask = health_stats.low_stack_mask;
        snapshot.reset_flags = boot_record.reset_flags;
        snapshot.last_fault_type = (uint8_t)boot_record.type;
        snapshot.watchdog_enabled = health_stats.watchdog_enabled;
        snapshot.ch9350_connection_known = ch9350_stats.connection_known;
        snapshot.ch9350_mouse_connected = ch9350_stats.mouse_connected;
        snapshot.ch9350_last_state_value = ch9350_stats.last_state_value;

        taskENTER_CRITICAL();
        g_monitor_snapshot = snapshot;
        taskEXIT_CRITICAL();

        if (snapshot.low_stack_mask != 0U &&
            snapshot.low_stack_mask != previous_low_stack_mask)
        {
            app_logs_add(APP_LOG_LEVEL_WARNING, "RTOS",
                         "TASK STACK LOW WATERMARK");
        }
        previous_low_stack_mask = snapshot.low_stack_mask;
        if (snapshot.minimum_heap_bytes < 32768U)
        {
            if (!low_heap_logged)
            {
                app_logs_add(APP_LOG_LEVEL_WARNING, "RTOS",
                             "FREE HEAP BELOW 32 KIB");
                low_heap_logged = 1U;
            }
        }
        else
        {
            low_heap_logged = 0U;
        }

        if (app_settings_get_serial_output_enabled())
        {
            printf("RTOS tick=%lu heap=%u queue=%u/%u input=%lu drop=%lu stack(I/G/M)=%u/%u/%u CH9350=%s report=%lu state=%lu last=0x%02X link=%lu/%lu err=%lu/%lu/%lu\r\n",
                   (unsigned long)xTaskGetTickCount(),
                   (unsigned int)snapshot.free_heap_bytes,
                   (unsigned int)queue_depth,
                   (unsigned int)queue_high_water,
                   (unsigned long)input_stats.sent_count,
                   (unsigned long)input_stats.dropped_count,
                   (unsigned int)snapshot.input_stack_watermark,
                   (unsigned int)snapshot.runtime_stack_watermark,
                   (unsigned int)snapshot.monitor_stack_watermark,
                   ch9350_stats.connection_known ?
                       (ch9350_stats.mouse_connected ? "ON" : "OFF") :
                       "WAIT",
                   (unsigned long)ch9350_stats.mouse_report_count,
                   (unsigned long)ch9350_stats.state_frame_count,
                   (unsigned int)ch9350_stats.last_state_value,
                   (unsigned long)ch9350_stats.connect_event_count,
                   (unsigned long)ch9350_stats.disconnect_event_count,
                   (unsigned long)ch9350_stats.discarded_frame_count,
                   (unsigned long)ch9350_stats.sync_error_count,
                   (unsigned long)ch9350_stats.uart_dropped_count);
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(500U));
    }
}
