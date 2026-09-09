/**
 * @file app_input.h
 * @brief 把触摸和鼠标原始输入转换为供应用消费的统一输入事件。
 * @details 这是 app_input 模块的接口文件（App/app_input.h）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#ifndef APP_INPUT_H
/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define APP_INPUT_H

#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"

#define APP_LCD_WIDTH            800U
#define APP_LCD_HEIGHT           480U
#define APP_INPUT_QUEUE_LENGTH   32U
#define APP_INPUT_QUEUE_RESERVED 8U

#define APP_INPUT_SENSITIVITY_LOW       1U
#define APP_INPUT_SENSITIVITY_NORMAL    2U
#define APP_INPUT_SENSITIVITY_HIGH      3U

/** @brief 模块数据类型：描述本模块维护的状态、配置或数据快照。 */
typedef enum
{
    APP_INPUT_EVENT_DOWN = 0,
    APP_INPUT_EVENT_MOVE,
    APP_INPUT_EVENT_UP,
    APP_INPUT_EVENT_SCROLL,
    APP_INPUT_EVENT_BACK,
    APP_INPUT_EVENT_MOUSE_CONNECTED,
    APP_INPUT_EVENT_MOUSE_DISCONNECTED
} app_input_event_type_t;

typedef enum
{
    APP_INPUT_SOURCE_TOUCH = 0,
    APP_INPUT_SOURCE_MOUSE,
    APP_INPUT_SOURCE_TEST
} app_input_source_t;

typedef enum
{
    APP_INPUT_MODE_NORMAL = 0,
    APP_INPUT_MODE_BUSY,
    APP_INPUT_MODE_PRESSURE,
    APP_INPUT_MODE_PROTECT
} app_input_scheduler_mode_t;

typedef enum
{
    APP_INPUT_TEST_NONE = 0,
    APP_INPUT_TEST_CAPTURE,
    APP_INPUT_TEST_FLOOD
} app_input_test_mode_t;

typedef struct
{
    app_input_event_type_t type;
    app_input_source_t source;
    uint16_t x;
    uint16_t y;
    int8_t wheel;
    uint8_t buttons;
    uint32_t tick;
    uint32_t enqueue_tick;
} app_input_event_t;

typedef struct
{
    QueueHandle_t event_queue;
    uint8_t touch_available;
} app_input_task_context_t;

typedef struct
{
    uint32_t raw_event_count;
    uint32_t raw_move_count;
    uint32_t raw_scroll_count;
    uint32_t sent_count;
    uint32_t consumed_count;
    uint32_t producer_merged_count;
    uint32_t consumer_merged_count;
    uint32_t dropped_count;
    uint32_t throttled_move_count;
    uint32_t throttled_scroll_count;
    uint32_t expired_move_count;
    uint32_t dropped_move_count;
    uint32_t critical_drop_count;
    uint32_t latency_sample_count;
    uint32_t latency_average_ms;
    uint16_t latency_current_ms;
    uint16_t latency_max_ms;
    uint16_t queue_depth;
    uint16_t queue_high_water;
    uint16_t move_interval_ms;
    uint8_t scheduler_mode;
    uint8_t test_mode;
    uint8_t test_complete;
    uint8_t test_passed;
    uint16_t test_remaining_seconds;
    uint16_t test_queue_peak;
    uint16_t test_latency_max_ms;
    uint32_t test_raw_count;
    uint32_t test_merged_count;
    uint32_t test_dropped_count;
    uint32_t test_critical_drop_count;
} app_input_stats_t;

/**
 * @brief AppInputTask：作为 FreeRTOS 任务入口，循环处理事件、周期工作和运行状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param argument 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 * @warning 该入口具有特定中断或任务上下文，禁止执行不符合该上下文约束的操作。
 */
void AppInputTask(void *argument);
/**
 * @brief app_input_get_stats：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param stats 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_input_get_stats(app_input_stats_t *stats);
/**
 * @brief app_input_post_event：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param queue 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param event 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
BaseType_t app_input_post_event(QueueHandle_t queue,
                                const app_input_event_t *event);
/**
 * @brief 从统一输入队列接收事件，并折叠队首连续的同源 MOVE 事件。
 * @param queue 输入事件队列。
 * @param event 返回给 GUI 的最终事件。
 * @param timeout 等待首个事件的最长时间。
 * @return pdPASS 表示返回了事件，否则返回 pdFAIL。
 */
BaseType_t app_input_receive_event(QueueHandle_t queue,
                                   app_input_event_t *event,
                                   TickType_t timeout);
/** @brief 启动 60 秒人工采集或 10 秒合成洪泛测试。 */
BaseType_t app_input_test_start(app_input_test_mode_t mode);
/** @brief 清零输入调度统计；测试进行时不会执行。 */
void app_input_reset_stats(void);
/** @brief 刷新待发送 MOVE/SCROLL，并依据队列压力执行限流。 */
void app_input_scheduler_poll(QueueHandle_t queue, TickType_t now);
/**
 * @brief app_input_set_cursor_sensitivity：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param sensitivity 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_input_set_cursor_sensitivity(uint8_t sensitivity);
/**
 * @brief app_input_get_cursor_sensitivity：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t app_input_get_cursor_sensitivity(void);

#endif
