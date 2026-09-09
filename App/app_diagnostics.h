/**
 * @file app_diagnostics.h
 * @brief 收集并显示启动诊断信息，辅助定位外设和系统初始化故障。
 * @details 这是 app_diagnostics 模块的接口文件（App/app_diagnostics.h）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#ifndef APP_DIAGNOSTICS_H
/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define APP_DIAGNOSTICS_H

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

#define APP_DIAGNOSTIC_MODE 0U

/** @brief 模块数据类型：描述本模块维护的状态、配置或数据快照。 */
typedef struct
{
    QueueHandle_t input_queue;
    TaskHandle_t input_task;
} app_diagnostics_context_t;

/**
 * @brief AppDiagnosticsTask：作为 FreeRTOS 任务入口，循环处理事件、周期工作和运行状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param argument 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 * @warning 该入口具有特定中断或任务上下文，禁止执行不符合该上下文约束的操作。
 */
void AppDiagnosticsTask(void *argument);

#endif
