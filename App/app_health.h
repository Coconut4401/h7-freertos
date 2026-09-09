/**
 * @file app_health.h
 * @brief 周期检查任务与外设健康状态，并更新系统健康统计。
 * @details 这是 app_health 模块的接口文件（App/app_health.h）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#ifndef APP_HEALTH_H
/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define APP_HEALTH_H

#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

/** @brief 模块数据类型：描述本模块维护的状态、配置或数据快照。 */
typedef enum
{
    APP_HEALTH_INPUT = 0U,
    APP_HEALTH_RUNTIME,
    APP_HEALTH_STORAGE,
    APP_HEALTH_AUDIO,
    APP_HEALTH_MONITOR,
    APP_HEALTH_COUNT
} app_health_task_id_t;

typedef struct
{
    TaskHandle_t input_task;
    TaskHandle_t runtime_task;
    TaskHandle_t storage_task;
    TaskHandle_t audio_task;
    TaskHandle_t monitor_task;
} app_health_context_t;

typedef struct
{
    uint32_t beat_mask;
    uint32_t healthy_mask;
    uint32_t timeout_mask;
    uint32_t low_stack_mask;
    uint32_t stack_words[APP_HEALTH_COUNT];
    uint32_t health_stack_words;
    uint32_t timer_stack_words;
    uint32_t current_heap;
    uint32_t minimum_heap;
    uint8_t watchdog_enabled;
    uint8_t startup_complete;
} app_health_snapshot_t;

/**
 * @brief app_health_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_health_init(void);
/**
 * @brief app_health_beat：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param task_id 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_health_beat(app_health_task_id_t task_id);
/**
 * @brief app_health_get_snapshot：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param snapshot 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_health_get_snapshot(app_health_snapshot_t *snapshot);
/**
 * @brief AppHealthTask：作为 FreeRTOS 任务入口，循环处理事件、周期工作和运行状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param argument 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 * @warning 该入口具有特定中断或任务上下文，禁止执行不符合该上下文约束的操作。
 */
void AppHealthTask(void *argument);

#endif
