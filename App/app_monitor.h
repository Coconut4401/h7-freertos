#ifndef APP_MONITOR_H
#define APP_MONITOR_H

#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

typedef struct
{
    QueueHandle_t event_queue;
    TaskHandle_t input_task;
    TaskHandle_t runtime_task;
} app_monitor_context_t;

typedef struct
{
    uint32_t uptime_seconds;
    uint32_t free_heap_bytes;
    uint32_t input_event_count;
    uint32_t dropped_event_count;
    uint32_t ch9350_mouse_report_count;
    uint32_t ch9350_state_frame_count;
    uint32_t ch9350_connect_event_count;
    uint32_t ch9350_disconnect_event_count;
    uint32_t ch9350_discarded_frame_count;
    uint32_t ch9350_sync_error_count;
    uint32_t ch9350_uart_dropped_count;
    uint16_t queue_depth;
    uint16_t queue_high_water;
    uint16_t input_stack_watermark;
    uint16_t runtime_stack_watermark;
    uint16_t monitor_stack_watermark;
    uint8_t ch9350_connection_known;
    uint8_t ch9350_mouse_connected;
    uint8_t ch9350_last_state_value;
} app_monitor_snapshot_t;

void AppMonitorTask(void *argument);
void app_monitor_get_snapshot(app_monitor_snapshot_t *snapshot);

#endif
