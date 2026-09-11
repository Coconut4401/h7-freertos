/**
 * @file app_mouse.h
 * @brief 解析 CH9350 鼠标数据并维护指针位置和按键状态。
 * @details 这是 app_mouse 模块的接口文件（App/app_mouse.h）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#ifndef APP_MOUSE_H
/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define APP_MOUSE_H

#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "app_input.h"
#include "./BSP/CH9350/ch9350.h"

/**
 * @brief app_mouse_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param event_queue 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_mouse_init(QueueHandle_t event_queue);
/**
 * @brief app_mouse_set_position：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_mouse_set_position(uint16_t x, uint16_t y);
/**
 * @brief app_mouse_process_report：解析并处理当前事件或数据，根据结果推进模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param report 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_mouse_process_report(const ch9350_mouse_report_t *report);
/** Reset button tracking from a trusted report without applying its motion. */
void app_mouse_rebaseline(const ch9350_mouse_report_t *report);
/**
 * @brief app_mouse_disconnect：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_mouse_disconnect(void);
/** Confirm sustained button presses that have no follow-up mouse report. */
void app_mouse_poll(void);
/**
 * @brief app_mouse_flush：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_mouse_flush(void);

#endif
