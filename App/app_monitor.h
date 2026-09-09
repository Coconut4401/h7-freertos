/**
 * @file app_monitor.h
 * @brief 采集 CPU、内存、存储等运行指标并生成监控快照。
 * @details 这是 app_monitor 模块的接口文件（App/app_monitor.h）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#ifndef APP_MONITOR_H
/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define APP_MONITOR_H

#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

/** @brief 模块数据类型：描述本模块维护的状态、配置或数据快照。 */
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
    uint32_t minimum_heap_bytes;
    uint32_t input_event_count;
    uint32_t dropped_event_count;
    uint32_t raw_event_count;
    uint32_t raw_move_count;
    uint32_t producer_merged_count;
    uint32_t consumer_merged_count;
    uint32_t dropped_event_rate;
    uint32_t throttled_move_count;
    uint32_t throttled_scroll_count;
    uint32_t critical_drop_count;
    uint32_t expired_move_count;
    uint32_t dropped_move_count;
    uint32_t latency_average_ms;
    uint16_t latency_current_ms;
    uint16_t latency_max_ms;
    uint16_t move_interval_ms;
    uint8_t scheduler_mode;
    uint8_t input_test_mode;
    uint8_t input_test_complete;
    uint8_t input_test_passed;
    uint16_t input_test_remaining_seconds;
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
    uint16_t storage_stack_watermark;
    uint16_t audio_stack_watermark;
    uint16_t monitor_stack_watermark;
    uint16_t health_stack_watermark;
    uint16_t timer_stack_watermark;
    uint16_t storage_queue_depth;
    uint16_t storage_queue_peak;
    uint32_t storage_queue_full_count;
    uint32_t storage_response_drop_count;
    uint32_t storage_error_count;
    uint32_t healthy_task_mask;
    uint32_t task_timeout_mask;
    uint32_t low_stack_mask;
    uint32_t reset_flags;
    uint8_t last_fault_type;
    uint8_t ch9350_connection_known;
    uint8_t ch9350_mouse_connected;
    uint8_t ch9350_last_state_value;
    uint8_t watchdog_enabled;
} app_monitor_snapshot_t;

/**
 * @brief AppMonitorTask：作为 FreeRTOS 任务入口，循环处理事件、周期工作和运行状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param argument 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 * @warning 该入口具有特定中断或任务上下文，禁止执行不符合该上下文约束的操作。
 */
void AppMonitorTask(void *argument);
/**
 * @brief app_monitor_get_snapshot：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param snapshot 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_monitor_get_snapshot(app_monitor_snapshot_t *snapshot);

#endif
