/**
 * @file app_audio.h
 * @brief 管理音频播放任务、缓冲区供给、播放状态和底层音频设备协作。
 * @details 这是 app_audio 模块的接口文件（App/app_audio.h）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#ifndef APP_AUDIO_H
/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define APP_AUDIO_H

#include <stdint.h>

#include "FreeRTOS.h"
#include "app_storage.h"

/** @brief 模块数据类型：描述本模块维护的状态、配置或数据快照。 */
typedef enum
{
    APP_AUDIO_STATE_STARTING = 0,
    APP_AUDIO_STATE_STOPPED,
    APP_AUDIO_STATE_PLAYING,
    APP_AUDIO_STATE_PAUSED,
    APP_AUDIO_STATE_TEST_TONE,
    APP_AUDIO_STATE_NO_TRACKS,
    APP_AUDIO_STATE_ERROR
} app_audio_state_t;

typedef enum
{
    APP_AUDIO_COMMAND_RESCAN = 0,
    APP_AUDIO_COMMAND_PLAY_PAUSE,
    APP_AUDIO_COMMAND_STOP,
    APP_AUDIO_COMMAND_PREVIOUS,
    APP_AUDIO_COMMAND_NEXT,
    APP_AUDIO_COMMAND_TEST_TONE,
    APP_AUDIO_COMMAND_SEEK_BACK_5S,
    APP_AUDIO_COMMAND_SEEK_FORWARD_5S
} app_audio_command_t;

typedef struct
{
    uint32_t revision;
    app_audio_state_t state;
    uint8_t track_count;
    uint8_t selected_track;
    uint8_t channels;
    uint8_t bits_per_sample;
    uint8_t volume_percent;
    uint8_t seek_available;
    uint8_t completed;
    uint32_t sample_rate;
    uint32_t data_size;
    uint32_t data_loaded;
    char track_name[APP_STORAGE_NAME_LENGTH];
    char status[48];
} app_audio_snapshot_t;

/**
 * @brief app_audio_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
BaseType_t app_audio_init(void);
/**
 * @brief app_audio_submit：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param command 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
BaseType_t app_audio_submit(app_audio_command_t command);
/**
 * @brief app_audio_set_volume：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param volume_percent 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_audio_set_volume(uint8_t volume_percent);
/**
 * @brief app_audio_get_volume：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t app_audio_get_volume(void);
/**
 * @brief app_audio_get_snapshot：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param snapshot 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_audio_get_snapshot(app_audio_snapshot_t *snapshot);
/**
 * @brief AppAudioTask：作为 FreeRTOS 任务入口，循环处理事件、周期工作和运行状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param argument 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 * @warning 该入口具有特定中断或任务上下文，禁止执行不符合该上下文约束的操作。
 */
void AppAudioTask(void *argument);
/**
 * @brief DMA1_Stream0_IRQHandler：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 * @warning 该入口具有特定中断或任务上下文，禁止执行不符合该上下文约束的操作。
 */
void DMA1_Stream0_IRQHandler(void);

#endif
