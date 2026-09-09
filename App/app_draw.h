/**
 * @file app_draw.h
 * @brief 实现绘图应用的画布、工具选择和触摸绘制逻辑。
 * @details 这是 app_draw 模块的接口文件（App/app_draw.h）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#ifndef APP_DRAW_H
/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define APP_DRAW_H

#include "app_input.h"

/**
 * @brief app_draw_open：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_draw_open(void);
/**
 * @brief app_draw_close：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_draw_close(void);
/** Handle BACK while a sample dialog or rename editor is active. */
uint8_t app_draw_handle_back(void);
/**
 * @brief app_draw_handle_event：解析并处理当前事件或数据，根据结果推进模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param event 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_draw_handle_event(const app_input_event_t *event);
/**
 * @brief app_draw_update：使用最新数据更新缓存、硬件输出或界面显示状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_draw_update(void);

#endif
