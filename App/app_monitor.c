/**
 * @file app_monitor.c
 * @brief 采集 CPU、内存、存储等运行指标并生成监控快照。
 * @details 这是 app_monitor 模块的实现文件（App/app_monitor.c）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

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

/**
 * @brief app_monitor_get_snapshot：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param snapshot 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
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

/**
 * @brief AppMonitorTask：作为 FreeRTOS 任务入口，循环处理事件、周期工作和运行状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param argument 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 * @warning 该入口具有特定中断或任务上下文，禁止执行不符合该上下文约束的操作。
 */
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
    app_monitor_snapshot_t snapshot;
    uint32_t previous_low_stack_mask;
    uint32_t previous_dropped_count;
    TickType_t previous_drop_tick;
    uint8_t low_heap_logged;
    uint8_t previous_test_complete;

    context = (const app_monitor_context_t *)argument;
    previous_low_stack_mask = 0U;
    previous_dropped_count = 0U;
    previous_drop_tick = 0U;
    low_heap_logged = 0U;
    previous_test_complete = 0U;
    last_wake = xTaskGetTickCount();

    while (1)
    {
        app_health_beat(APP_HEALTH_MONITOR);
        LED1_TOGGLE();
        queue_depth = uxQueueMessagesWaiting(context->event_queue);
        app_input_get_stats(&input_stats);
        {
            TickType_t now_tick;
            TickType_t elapsed_ticks;

            now_tick = xTaskGetTickCount();
            elapsed_ticks = (TickType_t)(now_tick - previous_drop_tick);
            snapshot.dropped_event_rate = 0U;
            if (input_stats.dropped_count < previous_dropped_count)
            {
                previous_dropped_count = input_stats.dropped_count;
            }
            if (previous_drop_tick != 0U && elapsed_ticks != 0U)
            {
                snapshot.dropped_event_rate = (uint32_t)(
                    ((uint64_t)(input_stats.dropped_count -
                                previous_dropped_count) *
                     (uint64_t)configTICK_RATE_HZ) /
                    (uint64_t)elapsed_ticks);
            }
            previous_dropped_count = input_stats.dropped_count;
            previous_drop_tick = now_tick;
        }
        ch9350_get_stats(&ch9350_stats);
        app_health_get_snapshot(&health_stats);
        app_storage_get_stats(&storage_stats);
        app_fault_get_boot_record(&boot_record);

        snapshot.uptime_seconds = (uint32_t)(xTaskGetTickCount() /
                                             configTICK_RATE_HZ);
        snapshot.free_heap_bytes = health_stats.current_heap;
        snapshot.minimum_heap_bytes = health_stats.minimum_heap;
        snapshot.input_event_count = input_stats.sent_count;
        snapshot.raw_event_count = input_stats.raw_event_count;
        snapshot.raw_move_count = input_stats.raw_move_count;
        snapshot.producer_merged_count = input_stats.producer_merged_count;
        snapshot.consumer_merged_count = input_stats.consumer_merged_count;
        snapshot.dropped_event_count = input_stats.dropped_count;
        snapshot.throttled_move_count = input_stats.throttled_move_count;
        snapshot.throttled_scroll_count = input_stats.throttled_scroll_count;
        snapshot.critical_drop_count = input_stats.critical_drop_count;
        snapshot.expired_move_count = input_stats.expired_move_count;
        snapshot.dropped_move_count = input_stats.dropped_move_count;
        snapshot.latency_average_ms = input_stats.latency_average_ms;
        snapshot.latency_current_ms = input_stats.latency_current_ms;
        snapshot.latency_max_ms = input_stats.latency_max_ms;
        snapshot.move_interval_ms = input_stats.move_interval_ms;
        snapshot.scheduler_mode = input_stats.scheduler_mode;
        snapshot.input_test_mode = input_stats.test_mode;
        snapshot.input_test_complete = input_stats.test_complete;
        snapshot.input_test_passed = input_stats.test_passed;
        snapshot.input_test_remaining_seconds =
            input_stats.test_remaining_seconds;
        snapshot.ch9350_mouse_report_count = ch9350_stats.mouse_report_count;
        snapshot.ch9350_state_frame_count = ch9350_stats.state_frame_count;
        snapshot.ch9350_connect_event_count = ch9350_stats.connect_event_count;
        snapshot.ch9350_disconnect_event_count =
            ch9350_stats.disconnect_event_count;
        snapshot.ch9350_discarded_frame_count =
            ch9350_stats.discarded_frame_count;
        snapshot.ch9350_sync_error_count = ch9350_stats.sync_error_count;
        snapshot.ch9350_uart_dropped_count = ch9350_stats.uart_dropped_count;
        snapshot.ch9350_uart_error_count = ch9350_stats.uart_error_count;
        snapshot.queue_depth = input_stats.queue_depth;
        snapshot.queue_high_water = input_stats.queue_high_water;
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
        if (snapshot.input_test_complete && !previous_test_complete)
        {
            app_logs_add(snapshot.input_test_passed ? APP_LOG_LEVEL_INFO :
                                                   APP_LOG_LEVEL_ERROR,
                         "INPUT",
                         snapshot.input_test_passed ?
                             "INPUT TEST PASS" : "INPUT TEST FAIL");
        }
        previous_test_complete = snapshot.input_test_complete;
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
            printf("RTOS tick=%lu heap=%u queue=%u/%u raw=%lu sent=%lu merge=%lu drop=%lu drop/s=%lu lat=%u/%u mode=%u stack(I/G/M)=%u/%u/%u CH9350=%s report=%lu state=%lu last=0x%02X link=%lu/%lu err=%lu/%lu/%lu/%lu\r\n",
                   (unsigned long)xTaskGetTickCount(),
                   (unsigned int)snapshot.free_heap_bytes,
                   (unsigned int)queue_depth,
                   (unsigned int)snapshot.queue_high_water,
                   (unsigned long)input_stats.raw_event_count,
                   (unsigned long)input_stats.sent_count,
                   (unsigned long)(input_stats.producer_merged_count +
                                   input_stats.consumer_merged_count),
                   (unsigned long)input_stats.dropped_count,
                   (unsigned long)snapshot.dropped_event_rate,
                   (unsigned int)snapshot.latency_average_ms,
                   (unsigned int)snapshot.latency_max_ms,
                   (unsigned int)snapshot.scheduler_mode,
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
                   (unsigned long)ch9350_stats.uart_dropped_count,
                   (unsigned long)ch9350_stats.uart_error_count);
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(500U));
    }
}
