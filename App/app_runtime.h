/**
 * @file app_runtime.h
 * @brief 驱动启动、登录、桌面、锁屏和应用切换等主状态机。
 * @details 这是 app_runtime 模块的接口文件（App/app_runtime.h）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#ifndef APP_RUNTIME_H
/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define APP_RUNTIME_H

#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"

/** @brief 模块数据类型：描述本模块维护的状态、配置或数据快照。 */
typedef enum
{
    APP_STATE_BOOT = 0,
    APP_STATE_LOGIN,
    APP_STATE_LOCKED,
    APP_STATE_DESKTOP,
    APP_STATE_APPLICATION,
    APP_STATE_SCREEN_OFF
} app_state_t;

typedef struct
{
    QueueHandle_t event_queue;
    uint8_t touch_available;
    const char *controller_id;
} app_runtime_context_t;

/**
 * @brief AppRuntimeTask：作为 FreeRTOS 任务入口，循环处理事件、周期工作和运行状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param argument 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 * @warning 该入口具有特定中断或任务上下文，禁止执行不符合该上下文约束的操作。
 */
void AppRuntimeTask(void *argument);

#endif
