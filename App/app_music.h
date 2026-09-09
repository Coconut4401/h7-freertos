/**
 * @file app_music.h
 * @brief 实现音乐应用的曲目选择、播放控制和界面联动。
 * @details 这是 app_music 模块的接口文件（App/app_music.h）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#ifndef APP_MUSIC_H
/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define APP_MUSIC_H

#include "app_input.h"

/**
 * @brief app_music_open：启动或启用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_music_open(void);
/**
 * @brief app_music_close：停止或禁用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_music_close(void);
/**
 * @brief app_music_handle_event：解析并处理当前事件或数据，根据结果推进模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param event 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_music_handle_event(const app_input_event_t *event);
/**
 * @brief app_music_update：使用最新数据更新缓存、硬件输出或界面显示状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_music_update(void);

#endif
