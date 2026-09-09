/**
 * @file ch9350.h
 * @brief 配置并读取 CH9350 USB 主机芯片，解析其串口 HID 数据。
 * @details 这是 ch9350 模块的接口文件（Drivers/BSP/CH9350/ch9350.h）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#ifndef CH9350_H
/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define CH9350_H

#include <stdint.h>

/** @brief 模块数据类型：描述本模块维护的状态、配置或数据快照。 */
typedef struct
{
    uint8_t buttons;
    int8_t delta_x;
    int8_t delta_y;
    int8_t wheel;
} ch9350_mouse_report_t;

typedef enum
{
    CH9350_EVENT_MOUSE_REPORT = 0,
    CH9350_EVENT_CONNECTION
} ch9350_event_type_t;

typedef struct
{
    ch9350_event_type_t type;
    ch9350_mouse_report_t mouse_report;
    uint8_t connection_changed;
    uint8_t mouse_connected;
} ch9350_event_t;

typedef struct
{
    uint32_t mouse_report_count;
    uint32_t state_frame_count;
    uint32_t connect_event_count;
    uint32_t disconnect_event_count;
    uint32_t discarded_frame_count;
    uint32_t sync_error_count;
    uint32_t uart_dropped_count;
    uint8_t connection_known;
    uint8_t mouse_connected;
    uint8_t last_state_value;
} ch9350_stats_t;

/**
 * @brief ch9350_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void ch9350_init(void);
/**
 * @brief ch9350_read_event：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param event 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t ch9350_read_event(ch9350_event_t *event);
/**
 * @brief ch9350_get_stats：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param stats 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void ch9350_get_stats(ch9350_stats_t *stats);

#endif
