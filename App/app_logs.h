/**
 * @file app_logs.h
 * @brief 维护运行日志缓冲区并向日志界面提供筛选和显示数据。
 * @details 这是 app_logs 模块的接口文件（App/app_logs.h）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#ifndef APP_LOGS_H
/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define APP_LOGS_H

#include <stdint.h>

#include "app_input.h"

#define APP_LOG_MAX_ENTRIES       64U
#define APP_LOG_MODULE_LENGTH     9U
#define APP_LOG_MESSAGE_LENGTH    49U

/** @brief 模块数据类型：描述本模块维护的状态、配置或数据快照。 */
typedef enum
{
    APP_LOG_LEVEL_INFO = 0,
    APP_LOG_LEVEL_WARNING,
    APP_LOG_LEVEL_ERROR
} app_log_level_t;

typedef struct
{
    uint32_t uptime_seconds;
    app_log_level_t level;
    char module[APP_LOG_MODULE_LENGTH];
    char message[APP_LOG_MESSAGE_LENGTH];
} app_log_entry_t;

/**
 * @brief app_logs_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_logs_init(void);
/**
 * @brief app_logs_add：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param level 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param module 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param message 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_logs_add(app_log_level_t level,
                  const char *module,
                  const char *message);
/**
 * @brief app_logs_open：启动或启用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_logs_open(void);
/**
 * @brief app_logs_close：停止或禁用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_logs_close(void);
/**
 * @brief app_logs_handle_event：解析并处理当前事件或数据，根据结果推进模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param event 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_logs_handle_event(const app_input_event_t *event);
/**
 * @brief app_logs_update：使用最新数据更新缓存、硬件输出或界面显示状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_logs_update(void);

#endif
