/**
 * @file app_storage.h
 * @brief 管理 FatFs 挂载、存储任务、文件访问和持久化数据。
 * @details 这是 app_storage 模块的接口文件（App/app_storage.h）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#ifndef APP_STORAGE_H
/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define APP_STORAGE_H

#include <stdint.h>

#include "FreeRTOS.h"

#define APP_STORAGE_MAX_FILES       24U
#define APP_STORAGE_NAME_LENGTH     13U
#define APP_STORAGE_TYPE_LENGTH     8U
#define APP_STORAGE_PATH_LENGTH     17U
#define APP_STORAGE_CONTENT_SIZE    256U
#define APP_STORAGE_AUDIO_MAX_TRACKS 8U

/** @brief 模块数据类型：描述本模块维护的状态、配置或数据快照。 */
typedef enum
{
    APP_STORAGE_STATE_STARTING = 0,
    APP_STORAGE_STATE_READY,
    APP_STORAGE_STATE_ERROR
} app_storage_state_t;

typedef enum
{
    APP_STORAGE_OP_LIST = 0,
    APP_STORAGE_OP_CREATE,
    APP_STORAGE_OP_READ,
    APP_STORAGE_OP_WRITE,
    APP_STORAGE_OP_DELETE,
    APP_STORAGE_OP_READ_BINARY,
    APP_STORAGE_OP_WRITE_BINARY,
    APP_STORAGE_OP_WRITE_LOG,
    APP_STORAGE_OP_READ_SETTINGS,
    APP_STORAGE_OP_WRITE_SETTINGS,
    APP_STORAGE_OP_AUDIO_SCAN,
    APP_STORAGE_OP_AUDIO_OPEN,
    APP_STORAGE_OP_AUDIO_READ,
    APP_STORAGE_OP_AUDIO_SEEK,
    APP_STORAGE_OP_AUDIO_CLOSE,
    APP_STORAGE_OP_WRITE_FAULT,
    APP_STORAGE_OP_RENAME,
    /** Dedicated drawing sample directory query, returned on binary queue. */
    APP_STORAGE_OP_DRAW_LIST,
    /** Dedicated drawing sample rename, returned on binary queue. */
    APP_STORAGE_OP_DRAW_RENAME
} app_storage_operation_t;

typedef enum
{
    APP_STORAGE_RESULT_OK = 0,
    APP_STORAGE_RESULT_NOT_READY,
    APP_STORAGE_RESULT_EXISTS,
    APP_STORAGE_RESULT_NOT_FOUND,
    APP_STORAGE_RESULT_INVALID_NAME,
    APP_STORAGE_RESULT_IO_ERROR,
    APP_STORAGE_RESULT_QUEUE_FULL
} app_storage_result_t;

typedef struct
{
    char name[APP_STORAGE_NAME_LENGTH];
    char type[APP_STORAGE_TYPE_LENGTH];
    char location[APP_STORAGE_PATH_LENGTH];
    uint32_t size;
    uint8_t active;
} app_storage_file_t;

typedef struct
{
    /** Monotonically increasing caller token used to reject stale responses. */
    uint32_t request_id;
    app_storage_operation_t operation;
    char name[APP_STORAGE_NAME_LENGTH];
    /** Destination name for APP_STORAGE_OP_RENAME. */
    char new_name[APP_STORAGE_NAME_LENGTH];
    char content[APP_STORAGE_CONTENT_SIZE];
    uint16_t content_length;
    void *binary_data;
    uint32_t binary_length;
    uint32_t binary_capacity;
    /** Byte offset relative to the start of the WAV data chunk. */
    uint32_t data_offset;
} app_storage_request_t;

typedef struct
{
    uint32_t request_id;
    app_storage_operation_t operation;
    app_storage_result_t result;
    uint8_t filesystem_result;
    uint32_t data_length;
    uint8_t draw_count;
    char draw_names[8][APP_STORAGE_NAME_LENGTH];
} app_storage_binary_response_t;

typedef struct
{
    uint32_t request_id;
    app_storage_operation_t operation;
    app_storage_result_t result;
    uint8_t filesystem_result;
    uint8_t track_count;
    uint8_t channels;
    uint8_t bits_per_sample;
    uint8_t end_of_file;
    uint32_t sample_rate;
    uint32_t data_size;
    uint32_t data_length;
    /** Actual frame-aligned byte offset used by AUDIO_SEEK. */
    uint32_t data_offset;
    char tracks[APP_STORAGE_AUDIO_MAX_TRACKS][APP_STORAGE_NAME_LENGTH];
} app_storage_audio_response_t;

typedef struct
{
    uint32_t request_id;
    app_storage_operation_t operation;
    app_storage_result_t result;
    uint8_t filesystem_result;
    uint8_t file_count;
    uint16_t content_length;
    uint32_t file_size;
    char name[APP_STORAGE_NAME_LENGTH];
    char content[APP_STORAGE_CONTENT_SIZE];
    app_storage_file_t files[APP_STORAGE_MAX_FILES];
} app_storage_response_t;

typedef struct
{
    uint32_t request_queue_peak;
    uint32_t request_queue_full_count;
    uint32_t response_drop_count;
    uint32_t filesystem_error_count;
    uint16_t request_queue_depth;
} app_storage_stats_t;

/**
 * @brief app_storage_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
BaseType_t app_storage_init(void);
/**
 * @brief app_storage_submit：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param request 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
BaseType_t app_storage_submit(const app_storage_request_t *request);
/**
 * @brief app_storage_receive：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param response 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
BaseType_t app_storage_receive(app_storage_response_t *response);
/**
 * @brief app_storage_receive_binary：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param response 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
BaseType_t app_storage_receive_binary(app_storage_binary_response_t *response);
/**
 * @brief app_storage_receive_log：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param response 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
BaseType_t app_storage_receive_log(app_storage_binary_response_t *response);
/**
 * @brief app_storage_receive_settings：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param response 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
BaseType_t app_storage_receive_settings(app_storage_binary_response_t *response);
/**
 * @brief app_storage_receive_audio：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param response 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
BaseType_t app_storage_receive_audio(app_storage_audio_response_t *response);
/**
 * @brief app_storage_receive_fault：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param response 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
BaseType_t app_storage_receive_fault(app_storage_binary_response_t *response);
/**
 * @brief app_storage_get_stats：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param stats 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_storage_get_stats(app_storage_stats_t *stats);
/**
 * @brief app_storage_get_state：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
app_storage_state_t app_storage_get_state(void);
/**
 * @brief app_storage_get_capacity_mb：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint32_t app_storage_get_capacity_mb(void);
/**
 * @brief AppStorageTask：作为 FreeRTOS 任务入口，循环处理事件、周期工作和运行状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param argument 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 * @warning 该入口具有特定中断或任务上下文，禁止执行不符合该上下文约束的操作。
 */
void AppStorageTask(void *argument);

#endif
