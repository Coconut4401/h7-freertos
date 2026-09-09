/**
 * @file app_fault.h
 * @brief 记录系统故障、维护故障快照并提供异常处理入口。
 * @details 这是 app_fault 模块的接口文件（App/app_fault.h）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#ifndef APP_FAULT_H
/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define APP_FAULT_H

#include <stdint.h>

/** @brief 模块数据类型：描述本模块维护的状态、配置或数据快照。 */
typedef enum
{
    APP_FAULT_NONE = 0U,
    APP_FAULT_ASSERT = 1U,
    APP_FAULT_STACK_OVERFLOW = 2U,
    APP_FAULT_MALLOC_FAILED = 3U,
    APP_FAULT_HARDFAULT = 4U,
    APP_FAULT_BUSFAULT = 5U,
    APP_FAULT_USAGEFAULT = 6U,
    APP_FAULT_MEMMANAGE = 7U,
    APP_FAULT_HEALTH_TIMEOUT = 8U
} app_fault_type_t;

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t sequence;
    uint32_t type;
    uint32_t tick;
    uint32_t reset_flags;
    uint32_t cfsr;
    uint32_t hfsr;
    uint32_t dfsr;
    uint32_t afsr;
    uint32_t bfar;
    uint32_t mmfar;
    uint32_t stacked_pc;
    uint32_t stacked_lr;
    uint32_t stacked_sp;
    char task[16];
    uint32_t checksum;
} app_fault_record_t;

/**
 * @brief app_fault_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_fault_init(void);
/**
 * @brief app_fault_get_pending：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param record 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t app_fault_get_pending(app_fault_record_t *record);
/**
 * @brief app_fault_get_boot_record：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param record 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_fault_get_boot_record(app_fault_record_t *record);
/**
 * @brief app_fault_clear：清除已有状态或复位目标设备，使其回到约定的初始状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_fault_clear(void);
/**
 * @brief app_fault_reported：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_fault_reported(void);
/**
 * @brief app_fault_publish_pending：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_fault_publish_pending(void);
/**
 * @brief app_fault_record：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param type 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param task_name 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_fault_record(app_fault_type_t type, const char *task_name);
/**
 * @brief app_fault_record_assert：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param file 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param line 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_fault_record_assert(const char *file, uint32_t line);
/**
 * @brief app_fault_assert_and_reset：清除已有状态或复位目标设备，使其回到约定的初始状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param file 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param line 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_fault_assert_and_reset(const char *file, uint32_t line);
/**
 * @brief app_fault_record_exception：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param type 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_fault_record_exception(app_fault_type_t type);
/**
 * @brief app_fault_panic：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param type 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param task_name 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_fault_panic(app_fault_type_t type, const char *task_name);

#endif
