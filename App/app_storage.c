/**
 * @file app_storage.c
 * @brief 管理 FatFs 挂载、存储任务、文件访问和持久化数据。
 * @details 这是 app_storage 模块的实现文件（App/app_storage.c）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#include "app_storage.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "app_logs.h"
#include "app_health.h"
#include "ff.h"
#include "./BSP/SDMMC/sdmmc_sdcard.h"

/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define STORAGE_REQUEST_QUEUE_LENGTH   8U
#define STORAGE_RESPONSE_QUEUE_LENGTH  2U
#define STORAGE_BINARY_QUEUE_LENGTH    2U
#define STORAGE_LOG_QUEUE_LENGTH       2U
#define STORAGE_SETTINGS_QUEUE_LENGTH  2U
#define STORAGE_AUDIO_QUEUE_LENGTH     2U
#define STORAGE_FAULT_QUEUE_LENGTH     1U
#define STORAGE_RESPONSE_TIMEOUT_MS    100U

#define STORAGE_BINARY_TEMP_PATH       "0:/~DRAW.TMP"
#define STORAGE_BINARY_BACKUP_PATH     "0:/~DRAW.BAK"
#define STORAGE_LOG_TEMP_PATH          "0:/~LOG.TMP"
#define STORAGE_LOG_BACKUP_PATH        "0:/~LOG.BAK"
#define STORAGE_SETTINGS_TEMP_PATH     "0:/~CFG.TMP"
#define STORAGE_SETTINGS_BACKUP_PATH   "0:/~CFG.BAK"
#define STORAGE_FAULT_TEMP_PATH        "0:/~FLT.TMP"
#define STORAGE_FAULT_BACKUP_PATH      "0:/~FLT.BAK"
#define STORAGE_TEXT_TEMP_PATH         "0:/~TXT.TMP"
#define STORAGE_TEXT_BACKUP_PATH       "0:/~TXT.BAK"
#define STORAGE_TEXT_JOURNAL_PATH      "0:/~TXT.JRN"
#define STORAGE_TEXT_JOURNAL_MAGIC     0x31545854UL

static FATFS g_sd_filesystem;
static QueueHandle_t g_request_queue;
static QueueHandle_t g_response_queue;
static QueueHandle_t g_binary_response_queue;
static QueueHandle_t g_log_response_queue;
static QueueHandle_t g_settings_response_queue;
static QueueHandle_t g_audio_response_queue;
static QueueHandle_t g_fault_response_queue;
static volatile uint32_t g_request_queue_peak;
static volatile uint32_t g_request_queue_full_count;
static volatile uint32_t g_response_drop_count;
static volatile uint32_t g_filesystem_error_count;
static volatile app_storage_state_t g_storage_state = APP_STORAGE_STATE_STARTING;
static volatile uint32_t g_capacity_mb;
static FIL g_audio_file;
static uint32_t g_audio_bytes_remaining;
static uint32_t g_audio_data_start;
static uint32_t g_audio_data_size;
static uint16_t g_audio_block_align;
static uint8_t g_audio_file_open;

static app_storage_result_t app_storage_map_result(FRESULT result);
static FRESULT app_storage_mount(void);
static FRESULT app_storage_recover_text_transaction(void);

/**
 * @brief app_storage_copy_text：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param destination 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param destination_size 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param source 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void app_storage_copy_text(char *destination,
                                  uint32_t destination_size,
                                  const char *source)
{
    uint32_t index;

    if (destination == NULL || destination_size == 0U)
    {
        return;
    }

    index = 0U;
    if (source != NULL)
    {
        while (source[index] != '\0' && index + 1U < destination_size)
        {
            destination[index] = source[index];
            index++;
        }
    }
    destination[index] = '\0';
}

static uint8_t app_storage_is_draw_name(const char *name)
{
    const char *extension;
    char a;
    char b;
    char c;

    if (name == NULL) return 0U;
    extension = strrchr(name, '.');
    if (extension == NULL || strlen(extension) != 4U) return 0U;
    a = extension[1]; b = extension[2]; c = extension[3];
    if (a >= 'a' && a <= 'z') a = (char)(a - ('a' - 'A'));
    if (b >= 'a' && b <= 'z') b = (char)(b - ('a' - 'A'));
    if (c >= 'a' && c <= 'z') c = (char)(c - ('a' - 'A'));
    return (a == 'D' && b == 'R' && c == 'W') ? 1U : 0U;
}

/**
 * @brief app_storage_name_is_valid：检查函数名所描述的条件是否成立，并返回明确的判断结果。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param name 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint8_t app_storage_name_is_valid(const char *name)
{
    uint32_t index;
    uint32_t length;

    if (name == NULL || name[0] == '\0')
    {
        return 0U;
    }
    if (name[0] == '~')
    {
        return 0U;
    }

    length = 0U;
    while (name[length] != '\0' && length < APP_STORAGE_NAME_LENGTH)
    {
        length++;
    }
    if (length == 0U || length >= APP_STORAGE_NAME_LENGTH)
    {
        return 0U;
    }

    for (index = 0U; index < length; index++)
    {
        if (name[index] == '/' || name[index] == '\\' ||
            name[index] == ':' || name[index] == '*' ||
            name[index] == '?' || name[index] == '"' ||
            name[index] == '<' || name[index] == '>' ||
            name[index] == '|')
        {
            return 0U;
        }
    }

    return 1U;
}

/**
 * @brief app_storage_make_path：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param path 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param name 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void app_storage_make_path(char path[APP_STORAGE_PATH_LENGTH],
                                  const char *name)
{
    path[0] = '0';
    path[1] = ':';
    path[2] = '/';
    app_storage_copy_text(&path[3], APP_STORAGE_PATH_LENGTH - 3U, name);
}

/**
 * @brief app_storage_set_file_type：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param file 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void app_storage_set_file_type(app_storage_file_t *file)
{
    const char *extension;

    extension = strrchr(file->name, '.');
    if (extension != NULL &&
        (strcmp(extension, ".TXT") == 0 || strcmp(extension, ".txt") == 0))
    {
        app_storage_copy_text(file->type, sizeof(file->type), "TEXT");
    }
    else if (extension != NULL &&
             (strcmp(extension, ".WAV") == 0 || strcmp(extension, ".wav") == 0))
    {
        app_storage_copy_text(file->type, sizeof(file->type), "AUDIO");
    }
    else
    {
        app_storage_copy_text(file->type, sizeof(file->type), "DATA");
    }
}

/**
 * @brief app_storage_read_u16：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param data 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint16_t app_storage_read_u16(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8U));
}

/**
 * @brief app_storage_read_u32：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param data 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint32_t app_storage_read_u32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8U) |
           ((uint32_t)data[2] << 16U) | ((uint32_t)data[3] << 24U);
}

/**
 * @brief app_storage_is_wav_name：检查函数名所描述的条件是否成立，并返回明确的判断结果。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param name 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint8_t app_storage_is_wav_name(const char *name)
{
    const char *extension;

    extension = strrchr(name, '.');
    if (extension == NULL || strlen(extension) != 4U)
    {
        return 0U;
    }
    return ((extension[1] == 'W' || extension[1] == 'w') &&
            (extension[2] == 'A' || extension[2] == 'a') &&
            (extension[3] == 'V' || extension[3] == 'v')) ? 1U : 0U;
}

/**
 * @brief app_storage_audio_rate_is_supported：检查函数名所描述的条件是否成立，并返回明确的判断结果。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param sample_rate 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint8_t app_storage_audio_rate_is_supported(uint32_t sample_rate)
{
    return (sample_rate == 16000U || sample_rate == 32000U ||
            sample_rate == 44100U || sample_rate == 48000U) ? 1U : 0U;
}

/**
 * @brief app_storage_audio_close_file：停止或禁用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
static void app_storage_audio_close_file(void)
{
    if (g_audio_file_open)
    {
        (void)f_close(&g_audio_file);
        g_audio_file_open = 0U;
    }
    g_audio_bytes_remaining = 0U;
    g_audio_data_start = 0U;
    g_audio_data_size = 0U;
    g_audio_block_align = 0U;
}

/**
 * @brief app_storage_audio_scan：扫描或采样当前输入与设备状态，整理本轮可用数据。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param response 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void app_storage_audio_scan(app_storage_audio_response_t *response)
{
    DIR directory;
    FILINFO information;
    FRESULT result;

    result = f_opendir(&directory, "0:/");
    if (result == FR_OK)
    {
        while (response->track_count < APP_STORAGE_AUDIO_MAX_TRACKS)
        {
            result = f_readdir(&directory, &information);
            if (result != FR_OK || information.fname[0] == '\0')
            {
                break;
            }
            if ((information.fattrib & AM_DIR) == 0U &&
                app_storage_is_wav_name(information.fname))
            {
                app_storage_copy_text(
                    response->tracks[response->track_count],
                    APP_STORAGE_NAME_LENGTH, information.fname);
                response->track_count++;
            }
        }
        (void)f_closedir(&directory);
    }

    response->filesystem_result = (uint8_t)result;
    response->result = app_storage_map_result(result);
}

/**
 * @brief app_storage_audio_open：启动或启用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param request 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param response 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void app_storage_audio_open(const app_storage_request_t *request,
                                   app_storage_audio_response_t *response)
{
    uint8_t header[16];
    uint8_t format_found;
    uint8_t data_found;
    uint16_t audio_format;
    uint16_t block_align;
    uint32_t chunk_size;
    uint32_t chunk_data_position;
    uint32_t file_size;
    uint32_t next_position;
    UINT transferred;
    FRESULT result;
    char path[APP_STORAGE_PATH_LENGTH];

    if (!app_storage_name_is_valid(request->name) ||
        !app_storage_is_wav_name(request->name))
    {
        response->result = APP_STORAGE_RESULT_INVALID_NAME;
        return;
    }

    app_storage_audio_close_file();
    app_storage_make_path(path, request->name);
    result = f_open(&g_audio_file, path, FA_READ);
    if (result != FR_OK)
    {
        response->filesystem_result = (uint8_t)result;
        response->result = app_storage_map_result(result);
        return;
    }
    g_audio_file_open = 1U;
    file_size = (uint32_t)f_size(&g_audio_file);

    transferred = 0U;
    result = f_read(&g_audio_file, header, 12U, &transferred);
    if (result != FR_OK || transferred != 12U ||
        memcmp(&header[0], "RIFF", 4U) != 0 ||
        memcmp(&header[8], "WAVE", 4U) != 0)
    {
        result = (result == FR_OK) ? FR_INVALID_OBJECT : result;
        goto audio_open_failed;
    }

    format_found = 0U;
    data_found = 0U;
    audio_format = 0U;
    block_align = 0U;
    while ((uint32_t)f_tell(&g_audio_file) <= file_size &&
           file_size - (uint32_t)f_tell(&g_audio_file) >= 8U)
    {
        result = f_read(&g_audio_file, header, 8U, &transferred);
        if (result != FR_OK || transferred != 8U)
        {
            break;
        }
        chunk_size = app_storage_read_u32(&header[4]);
        chunk_data_position = (uint32_t)f_tell(&g_audio_file);
        if (chunk_data_position > file_size ||
            chunk_size > file_size - chunk_data_position)
        {
            result = FR_INVALID_OBJECT;
            break;
        }
        next_position = chunk_data_position + chunk_size;
        if ((chunk_size & 1U) != 0U)
        {
            if (next_position >= file_size)
            {
                result = FR_INVALID_OBJECT;
                break;
            }
            next_position++;
        }
        if (memcmp(header, "fmt ", 4U) == 0 && chunk_size >= 16U)
        {
            result = f_read(&g_audio_file, header, 16U, &transferred);
            if (result != FR_OK || transferred != 16U)
            {
                break;
            }
            audio_format = app_storage_read_u16(&header[0]);
            response->channels = (uint8_t)app_storage_read_u16(&header[2]);
            response->sample_rate = app_storage_read_u32(&header[4]);
            block_align = app_storage_read_u16(&header[12]);
            response->bits_per_sample =
                (uint8_t)app_storage_read_u16(&header[14]);
            format_found = 1U;
        }
        else if (memcmp(header, "data", 4U) == 0 && format_found)
        {
            if (block_align == 0U)
            {
                result = FR_INVALID_OBJECT;
                break;
            }
            chunk_size -= chunk_size % block_align;
            if (chunk_size == 0U)
            {
                result = FR_INVALID_OBJECT;
                break;
            }
            response->data_size = chunk_size;
            g_audio_data_start = chunk_data_position;
            g_audio_data_size = chunk_size;
            g_audio_block_align = block_align;
            g_audio_bytes_remaining = chunk_size;
            data_found = 1U;
            break;
        }
        result = f_lseek(&g_audio_file, next_position);
        if (result != FR_OK)
        {
            break;
        }
    }

    if (result != FR_OK || !format_found || !data_found ||
        audio_format != 1U ||
        (response->channels != 1U && response->channels != 2U) ||
        response->bits_per_sample != 16U ||
        block_align != (uint16_t)(response->channels * 2U) ||
        !app_storage_audio_rate_is_supported(response->sample_rate))
    {
        result = (result == FR_OK) ? FR_INVALID_OBJECT : result;
        goto audio_open_failed;
    }

    response->filesystem_result = (uint8_t)FR_OK;
    response->result = APP_STORAGE_RESULT_OK;
    return;

audio_open_failed:
    app_storage_audio_close_file();
    response->filesystem_result = (uint8_t)result;
    response->result = app_storage_map_result(result);
}

/**
 * @brief app_storage_audio_read：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param request 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param response 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void app_storage_audio_read(const app_storage_request_t *request,
                                   app_storage_audio_response_t *response)
{
    UINT transferred;
    uint32_t requested;
    FRESULT result;

    if (!g_audio_file_open || request->binary_data == NULL ||
        request->binary_capacity == 0U)
    {
        response->result = APP_STORAGE_RESULT_NOT_READY;
        return;
    }

    requested = request->binary_capacity;
    if (requested > g_audio_bytes_remaining)
    {
        requested = g_audio_bytes_remaining;
    }
    transferred = 0U;
    result = f_read(&g_audio_file, request->binary_data,
                    requested, &transferred);
    if (result == FR_OK)
    {
        g_audio_bytes_remaining -= transferred;
        response->data_length = transferred;
        response->end_of_file = (g_audio_bytes_remaining == 0U) ? 1U : 0U;
    }
    else
    {
        app_storage_audio_close_file();
    }
    response->filesystem_result = (uint8_t)result;
    response->result = app_storage_map_result(result);
}

/** Seek to a byte offset within the current WAV data chunk. */
static void app_storage_audio_seek(const app_storage_request_t *request,
                                   app_storage_audio_response_t *response)
{
    uint32_t offset;
    FRESULT result;

    if (!g_audio_file_open || g_audio_block_align == 0U ||
        g_audio_data_size == 0U)
    {
        response->result = APP_STORAGE_RESULT_NOT_READY;
        return;
    }

    offset = request->data_offset;
    if (offset > g_audio_data_size)
    {
        offset = g_audio_data_size;
    }
    offset -= offset % g_audio_block_align;
    result = f_lseek(&g_audio_file, g_audio_data_start + offset);
    if (result == FR_OK)
    {
        g_audio_bytes_remaining = g_audio_data_size - offset;
        response->data_offset = offset;
        response->data_size = g_audio_data_size;
        response->end_of_file = (offset == g_audio_data_size) ? 1U : 0U;
    }
    else
    {
        app_storage_audio_close_file();
    }
    response->filesystem_result = (uint8_t)result;
    response->result = app_storage_map_result(result);
}

/**
 * @brief app_storage_process_audio：解析并处理当前事件或数据，根据结果推进模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param request 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param response 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void app_storage_process_audio(const app_storage_request_t *request,
                                      app_storage_audio_response_t *response)
{
    FRESULT mount_result;

    memset(response, 0, sizeof(*response));
    response->request_id = request->request_id;
    response->operation = request->operation;
    if (g_storage_state != APP_STORAGE_STATE_READY)
    {
        mount_result = app_storage_mount();
        if (mount_result != FR_OK)
        {
            response->filesystem_result = (uint8_t)mount_result;
            response->result = APP_STORAGE_RESULT_NOT_READY;
            return;
        }
    }

    if (request->operation == APP_STORAGE_OP_AUDIO_SCAN)
    {
        app_storage_audio_scan(response);
    }
    else if (request->operation == APP_STORAGE_OP_AUDIO_OPEN)
    {
        app_storage_audio_open(request, response);
    }
    else if (request->operation == APP_STORAGE_OP_AUDIO_READ)
    {
        app_storage_audio_read(request, response);
    }
    else if (request->operation == APP_STORAGE_OP_AUDIO_SEEK)
    {
        app_storage_audio_seek(request, response);
    }
    else if (request->operation == APP_STORAGE_OP_AUDIO_CLOSE)
    {
        app_storage_audio_close_file();
        response->result = APP_STORAGE_RESULT_OK;
    }
    else
    {
        response->result = APP_STORAGE_RESULT_IO_ERROR;
    }
}

/**
 * @brief app_storage_map_result：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param result 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static app_storage_result_t app_storage_map_result(FRESULT result)
{
    if (result == FR_OK)
    {
        return APP_STORAGE_RESULT_OK;
    }
    if (result == FR_EXIST)
    {
        return APP_STORAGE_RESULT_EXISTS;
    }
    if (result == FR_NO_FILE || result == FR_NO_PATH)
    {
        return APP_STORAGE_RESULT_NOT_FOUND;
    }
    if (result == FR_INVALID_NAME)
    {
        return APP_STORAGE_RESULT_INVALID_NAME;
    }
    if (result == FR_DISK_ERR || result == FR_INT_ERR ||
        result == FR_NOT_READY || result == FR_NOT_ENABLED ||
        result == FR_NO_FILESYSTEM || result == FR_TIMEOUT)
    {
        g_storage_state = APP_STORAGE_STATE_ERROR;
        return APP_STORAGE_RESULT_NOT_READY;
    }
    return APP_STORAGE_RESULT_IO_ERROR;
}

/**
 * @brief app_storage_mount：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static FRESULT app_storage_mount(void)
{
    FRESULT result;

    g_storage_state = APP_STORAGE_STATE_STARTING;
    result = f_mount(&g_sd_filesystem, "0:", 1U);
    if (result == FR_OK)
    {
        result = app_storage_recover_text_transaction();
    }
    if (result == FR_OK)
    {
        g_capacity_mb = (uint32_t)((uint64_t)SDCardInfo.CardCapacity >> 20);
        g_storage_state = APP_STORAGE_STATE_READY;
    }
    else
    {
        g_capacity_mb = 0U;
        g_storage_state = APP_STORAGE_STATE_ERROR;
    }
    return result;
}

/**
 * @brief app_storage_run_self_test：执行设备探测或自检，并将检测结果返回或记录给上层模块。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
static void app_storage_run_self_test(void)
{
    static const char expected_text[] = "STM32 SD TEST";
    static const char write_text[] = "STM32 SD WRITE OK\r\n";
    FIL file;
    FRESULT result;
    UINT transferred;
    char buffer[64];

    result = f_open(&file, "0:/TEST.TXT", FA_READ);
    if (result == FR_OK)
    {
        memset(buffer, 0, sizeof(buffer));
        result = f_read(&file, buffer, sizeof(buffer) - 1U, &transferred);
        f_close(&file);
        if (result == FR_OK && transferred >= sizeof(expected_text) - 1U &&
            memcmp(buffer, expected_text, sizeof(expected_text) - 1U) == 0)
        {
            printf("SD TEST.TXT content: PASS\r\n");
        }
        else
        {
            printf("SD TEST.TXT content: FAIL (%u)\r\n", (unsigned int)result);
        }
    }
    else
    {
        printf("SD TEST.TXT: OPEN FAIL (%u)\r\n", (unsigned int)result);
    }

    result = f_open(&file, "0:/STM32_OK.TXT", FA_CREATE_ALWAYS | FA_WRITE);
    if (result == FR_OK)
    {
        result = f_write(&file, write_text, sizeof(write_text) - 1U, &transferred);
        if (result == FR_OK && transferred == sizeof(write_text) - 1U)
        {
            result = f_sync(&file);
        }
        f_close(&file);
    }
    if (result == FR_OK && transferred == sizeof(write_text) - 1U)
    {
        printf("SD write/read verify: PASS\r\n");
    }
    else
    {
        printf("SD write/read verify: FAIL (%u)\r\n", (unsigned int)result);
    }
}

/**
 * @brief app_storage_sort_files：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param response 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void app_storage_sort_files(app_storage_response_t *response)
{
    uint8_t first;
    uint8_t second;
    app_storage_file_t temporary;

    for (first = 0U; first < response->file_count; first++)
    {
        for (second = (uint8_t)(first + 1U);
             second < response->file_count;
             second++)
        {
            if (strcmp(response->files[first].name,
                       response->files[second].name) > 0)
            {
                temporary = response->files[first];
                response->files[first] = response->files[second];
                response->files[second] = temporary;
            }
        }
    }
}

/**
 * @brief app_storage_list：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param response 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void app_storage_list(app_storage_response_t *response)
{
    DIR directory;
    FILINFO information;
    FRESULT result;
    app_storage_file_t *file;

    result = f_opendir(&directory, "0:/");
    if (result == FR_OK)
    {
        while (response->file_count < APP_STORAGE_MAX_FILES)
        {
            result = f_readdir(&directory, &information);
            if (result != FR_OK || information.fname[0] == '\0')
            {
                break;
            }
            if ((information.fattrib & AM_DIR) != 0U)
            {
                continue;
            }
            if (information.fname[0] == '~')
            {
                continue;
            }

            file = &response->files[response->file_count];
            app_storage_copy_text(file->name, sizeof(file->name), information.fname);
            app_storage_make_path(file->location, file->name);
            app_storage_set_file_type(file);
            file->size = (uint32_t)information.fsize;
            file->active = 1U;
            response->file_count++;
        }
        f_closedir(&directory);
    }

    response->filesystem_result = (uint8_t)result;
    response->result = app_storage_map_result(result);
    if (response->result == APP_STORAGE_RESULT_OK)
    {
        app_storage_sort_files(response);
    }
}

/**
 * @brief app_storage_create：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param request 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param response 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void app_storage_create(const app_storage_request_t *request,
                               app_storage_response_t *response)
{
    FIL file;
    FRESULT result;
    FRESULT close_result;
    UINT transferred;
    char path[APP_STORAGE_PATH_LENGTH];

    if (!app_storage_name_is_valid(request->name))
    {
        response->result = APP_STORAGE_RESULT_INVALID_NAME;
        return;
    }
    if (request->content_length >= APP_STORAGE_CONTENT_SIZE)
    {
        response->result = APP_STORAGE_RESULT_IO_ERROR;
        return;
    }

    app_storage_make_path(path, request->name);
    result = f_open(&file, path, FA_CREATE_NEW | FA_WRITE);
    transferred = 0U;
    if (result == FR_OK)
    {
        result = f_write(&file, request->content, request->content_length,
                         &transferred);
        if (result == FR_OK && transferred == request->content_length)
        {
            result = f_sync(&file);
        }
        else if (result == FR_OK)
        {
            result = FR_DISK_ERR;
        }
        close_result = f_close(&file);
        if (result == FR_OK)
        {
            result = close_result;
        }
        if (result != FR_OK)
        {
            (void)f_unlink(path);
        }
    }

    response->filesystem_result = (uint8_t)result;
    response->result = app_storage_map_result(result);
    app_storage_copy_text(response->name, sizeof(response->name), request->name);
}

/**
 * @brief app_storage_read：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param request 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param response 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void app_storage_read(const app_storage_request_t *request,
                             app_storage_response_t *response)
{
    FIL file;
    FRESULT result;
    UINT transferred;
    char path[APP_STORAGE_PATH_LENGTH];

    if (!app_storage_name_is_valid(request->name))
    {
        response->result = APP_STORAGE_RESULT_INVALID_NAME;
        return;
    }
    app_storage_make_path(path, request->name);
    result = f_open(&file, path, FA_READ);
    transferred = 0U;
    if (result == FR_OK)
    {
        response->file_size = (uint32_t)f_size(&file);
        result = f_read(&file, response->content,
                        APP_STORAGE_CONTENT_SIZE - 1U, &transferred);
        f_close(&file);
    }

    response->filesystem_result = (uint8_t)result;
    response->result = app_storage_map_result(result);
    response->content_length = (uint16_t)transferred;
    response->content[transferred] = '\0';
    app_storage_copy_text(response->name, sizeof(response->name), request->name);
}

/**
 * @brief app_storage_write：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param request 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param response 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
typedef struct
{
    uint32_t magic;
    uint32_t checksum;
    char target_name[APP_STORAGE_NAME_LENGTH];
    uint8_t reserved[3];
} app_storage_text_journal_t;

static uint32_t app_storage_text_journal_checksum(
    const char target_name[APP_STORAGE_NAME_LENGTH])
{
    uint32_t hash;
    uint32_t index;

    hash = 2166136261UL;
    for (index = 0U; index < APP_STORAGE_NAME_LENGTH; index++)
    {
        hash ^= (uint8_t)target_name[index];
        hash *= 16777619UL;
    }
    return hash;
}

static FRESULT app_storage_write_text_journal(const char *target_name)
{
    app_storage_text_journal_t journal;
    FIL file;
    FRESULT result;
    FRESULT close_result;
    UINT transferred;

    memset(&journal, 0, sizeof(journal));
    journal.magic = STORAGE_TEXT_JOURNAL_MAGIC;
    app_storage_copy_text(journal.target_name, sizeof(journal.target_name),
                          target_name);
    journal.checksum =
        app_storage_text_journal_checksum(journal.target_name);
    result = f_open(&file, STORAGE_TEXT_JOURNAL_PATH,
                    FA_CREATE_ALWAYS | FA_WRITE);
    if (result != FR_OK)
    {
        return result;
    }
    transferred = 0U;
    result = f_write(&file, &journal, sizeof(journal), &transferred);
    if (result == FR_OK && transferred == sizeof(journal))
    {
        result = f_sync(&file);
    }
    else if (result == FR_OK)
    {
        result = FR_DISK_ERR;
    }
    close_result = f_close(&file);
    return (result == FR_OK) ? close_result : result;
}

static FRESULT app_storage_recover_text_transaction(void)
{
    app_storage_text_journal_t journal;
    FIL file;
    FILINFO information;
    FRESULT result;
    FRESULT close_result;
    UINT transferred;
    char target_path[APP_STORAGE_PATH_LENGTH];

    result = f_open(&file, STORAGE_TEXT_JOURNAL_PATH, FA_READ);
    if (result == FR_NO_FILE || result == FR_NO_PATH)
    {
        /* A temporary file without a durable journal was never committed. */
        (void)f_unlink(STORAGE_TEXT_TEMP_PATH);
        return FR_OK;
    }
    if (result != FR_OK)
    {
        return result;
    }
    memset(&journal, 0, sizeof(journal));
    transferred = 0U;
    result = f_read(&file, &journal, sizeof(journal), &transferred);
    close_result = f_close(&file);
    if (result == FR_OK)
    {
        result = close_result;
    }
    if (result != FR_OK || transferred != sizeof(journal) ||
        journal.magic != STORAGE_TEXT_JOURNAL_MAGIC ||
        journal.checksum !=
            app_storage_text_journal_checksum(journal.target_name) ||
        journal.target_name[APP_STORAGE_NAME_LENGTH - 1U] != '\0' ||
        !app_storage_name_is_valid(journal.target_name))
    {
        return (result == FR_OK) ? FR_INT_ERR : result;
    }

    app_storage_make_path(target_path, journal.target_name);
    result = f_stat(target_path, &information);
    if (result == FR_OK)
    {
        /* The replacement reached its final name; only cleanup was pending. */
        (void)f_unlink(STORAGE_TEXT_TEMP_PATH);
        (void)f_unlink(STORAGE_TEXT_BACKUP_PATH);
        (void)f_unlink(STORAGE_TEXT_JOURNAL_PATH);
        return FR_OK;
    }
    if (result != FR_NO_FILE && result != FR_NO_PATH)
    {
        return result;
    }

    result = f_stat(STORAGE_TEXT_TEMP_PATH, &information);
    if (result == FR_OK)
    {
        result = f_rename(STORAGE_TEXT_TEMP_PATH, target_path);
        if (result == FR_OK)
        {
            (void)f_unlink(STORAGE_TEXT_BACKUP_PATH);
            (void)f_unlink(STORAGE_TEXT_JOURNAL_PATH);
        }
        return result;
    }
    if (result != FR_NO_FILE && result != FR_NO_PATH)
    {
        return result;
    }
    result = f_stat(STORAGE_TEXT_BACKUP_PATH, &information);
    if (result == FR_OK)
    {
        result = f_rename(STORAGE_TEXT_BACKUP_PATH, target_path);
        if (result == FR_OK)
        {
            (void)f_unlink(STORAGE_TEXT_JOURNAL_PATH);
        }
        return result;
    }
    if (result != FR_NO_FILE && result != FR_NO_PATH)
    {
        return result;
    }

    /* Nothing was renamed yet; abandoning the journal preserves the target's
     * absence and allows the storage service to remain usable. */
    (void)f_unlink(STORAGE_TEXT_JOURNAL_PATH);
    return FR_OK;
}

static void app_storage_write(const app_storage_request_t *request,
                              app_storage_response_t *response)
{
    FIL file;
    FILINFO information;
    FRESULT result;
    FRESULT stat_result;
    FRESULT close_result;
    UINT transferred;
    uint8_t had_old_file;
    char path[APP_STORAGE_PATH_LENGTH];

    if (!app_storage_name_is_valid(request->name))
    {
        response->result = APP_STORAGE_RESULT_INVALID_NAME;
        return;
    }
    if (request->content_length >= APP_STORAGE_CONTENT_SIZE)
    {
        response->result = APP_STORAGE_RESULT_IO_ERROR;
        return;
    }

    app_storage_make_path(path, request->name);
    transferred = 0U;
    result = app_storage_recover_text_transaction();
    if (result != FR_OK)
    {
        goto text_write_done;
    }
    (void)f_unlink(STORAGE_TEXT_TEMP_PATH);
    result = f_open(&file, STORAGE_TEXT_TEMP_PATH,
                    FA_CREATE_ALWAYS | FA_WRITE);
    if (result == FR_OK)
    {
        result = f_write(&file, request->content, request->content_length,
                         &transferred);
        if (result == FR_OK && transferred == request->content_length)
        {
            result = f_sync(&file);
        }
        else if (result == FR_OK)
        {
            result = FR_DISK_ERR;
        }
        close_result = f_close(&file);
        if (result == FR_OK)
        {
            result = close_result;
        }
    }
    if (result != FR_OK)
    {
        (void)f_unlink(STORAGE_TEXT_TEMP_PATH);
        goto text_write_done;
    }

    result = app_storage_write_text_journal(request->name);
    if (result != FR_OK)
    {
        (void)f_unlink(STORAGE_TEXT_TEMP_PATH);
        goto text_write_done;
    }
    stat_result = f_stat(path, &information);
    had_old_file = (stat_result == FR_OK) ? 1U : 0U;
    if (stat_result != FR_OK && stat_result != FR_NO_FILE &&
        stat_result != FR_NO_PATH)
    {
        result = stat_result;
        goto text_write_done;
    }
    if (had_old_file)
    {
        (void)f_unlink(STORAGE_TEXT_BACKUP_PATH);
        result = f_rename(path, STORAGE_TEXT_BACKUP_PATH);
    }
    if (result == FR_OK)
    {
        result = f_rename(STORAGE_TEXT_TEMP_PATH, path);
    }
    if (result == FR_OK)
    {
        (void)f_unlink(STORAGE_TEXT_BACKUP_PATH);
        (void)f_unlink(STORAGE_TEXT_JOURNAL_PATH);
    }
    else
    {
        /* The journal identifies the exact target, so recovery cannot restore
         * one text file into another even after a reset. */
        result = app_storage_recover_text_transaction();
    }

text_write_done:
    response->filesystem_result = (uint8_t)result;
    response->result = app_storage_map_result(result);
    response->content_length = (result == FR_OK) ?
                               request->content_length : (uint16_t)transferred;
    app_storage_copy_text(response->name, sizeof(response->name), request->name);
}

/**
 * @brief app_storage_delete：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param request 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param response 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void app_storage_delete(const app_storage_request_t *request,
                               app_storage_response_t *response)
{
    FRESULT result;
    char path[APP_STORAGE_PATH_LENGTH];

    if (!app_storage_name_is_valid(request->name))
    {
        response->result = APP_STORAGE_RESULT_INVALID_NAME;
        return;
    }

    app_storage_make_path(path, request->name);
    result = f_unlink(path);
    response->filesystem_result = (uint8_t)result;
    response->result = app_storage_map_result(result);
    app_storage_copy_text(response->name, sizeof(response->name), request->name);
}

static void app_storage_rename(const app_storage_request_t *request,
                               app_storage_response_t *response)
{
    FRESULT result;
    FILINFO information;
    char source_path[APP_STORAGE_PATH_LENGTH];
    char target_path[APP_STORAGE_PATH_LENGTH];

    if (!app_storage_name_is_valid(request->name) ||
        !app_storage_name_is_valid(request->new_name))
    {
        response->result = APP_STORAGE_RESULT_INVALID_NAME;
        return;
    }
    if (strcmp(request->name, request->new_name) == 0)
    {
        response->result = APP_STORAGE_RESULT_OK;
        app_storage_copy_text(response->name, sizeof(response->name),
                               request->new_name);
        return;
    }

    app_storage_make_path(source_path, request->name);
    app_storage_make_path(target_path, request->new_name);
    result = f_stat(source_path, &information);
    if (result != FR_OK)
    {
        response->filesystem_result = (uint8_t)result;
        response->result = app_storage_map_result(result);
        return;
    }
    /* Never replace an existing file: this keeps a mistyped rename
     * recoverable and avoids destroying user data. */
    result = f_stat(target_path, &information);
    if (result == FR_OK)
    {
        response->filesystem_result = (uint8_t)FR_EXIST;
        response->result = APP_STORAGE_RESULT_EXISTS;
        return;
    }
    if (result != FR_NO_FILE && result != FR_NO_PATH)
    {
        response->filesystem_result = (uint8_t)result;
        response->result = app_storage_map_result(result);
        return;
    }

    result = f_rename(source_path, target_path);
    response->filesystem_result = (uint8_t)result;
    response->result = app_storage_map_result(result);
    app_storage_copy_text(response->name, sizeof(response->name),
                          (result == FR_OK) ? request->new_name : request->name);
}

/**
 * @brief app_storage_recover_binary_target：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param path 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param temporary_path 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param backup_path 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static FRESULT app_storage_recover_binary_target(const char *path,
                                                  const char *temporary_path,
                                                  const char *backup_path)
{
    FILINFO information;
    FRESULT result;

    result = f_stat(path, &information);
    if (result == FR_OK)
    {
        return FR_OK;
    }
    if (result != FR_NO_FILE && result != FR_NO_PATH)
    {
        return result;
    }

    result = f_stat(backup_path, &information);
    if (result == FR_OK)
    {
        return f_rename(backup_path, path);
    }

    result = f_stat(temporary_path, &information);
    if (result == FR_OK)
    {
        return f_rename(temporary_path, path);
    }
    return FR_NO_FILE;
}

/**
 * @brief app_storage_read_binary：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param request 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param response 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void app_storage_read_binary(const app_storage_request_t *request,
                                    app_storage_binary_response_t *response)
{
    FIL file;
    FRESULT result;
    UINT transferred;
    uint32_t file_size;
    char path[APP_STORAGE_PATH_LENGTH];
    const char *temporary_path;
    const char *backup_path;

    if (!app_storage_name_is_valid(request->name) ||
        request->binary_data == NULL || request->binary_capacity == 0U)
    {
        response->result = APP_STORAGE_RESULT_INVALID_NAME;
        return;
    }

    if (request->operation == APP_STORAGE_OP_READ_SETTINGS)
    {
        temporary_path = STORAGE_SETTINGS_TEMP_PATH;
        backup_path = STORAGE_SETTINGS_BACKUP_PATH;
    }
    else
    {
        temporary_path = STORAGE_BINARY_TEMP_PATH;
        backup_path = STORAGE_BINARY_BACKUP_PATH;
    }

    app_storage_make_path(path, request->name);
    result = app_storage_recover_binary_target(path,
                                               temporary_path,
                                               backup_path);
    if (result != FR_OK)
    {
        response->filesystem_result = (uint8_t)result;
        response->result = app_storage_map_result(result);
        return;
    }

    result = f_open(&file, path, FA_READ);
    transferred = 0U;
    file_size = 0U;
    if (result == FR_OK)
    {
        file_size = (uint32_t)f_size(&file);
        if (file_size > request->binary_capacity)
        {
            result = FR_INVALID_OBJECT;
        }
        else
        {
            result = f_read(&file, request->binary_data, file_size,
                            &transferred);
        }
        f_close(&file);
    }

    response->filesystem_result = (uint8_t)result;
    response->result = app_storage_map_result(result);
    response->data_length = (result == FR_OK) ? (uint32_t)transferred : file_size;
}

/**
 * @brief app_storage_write_binary：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param request 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param response 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void app_storage_write_binary(const app_storage_request_t *request,
                                     app_storage_binary_response_t *response)
{
    FIL file;
    FILINFO information;
    FRESULT result;
    FRESULT old_result;
    UINT transferred;
    uint8_t had_old_file;
    char path[APP_STORAGE_PATH_LENGTH];
    const char *temporary_path;
    const char *backup_path;

    if (!app_storage_name_is_valid(request->name) ||
        request->binary_data == NULL || request->binary_length == 0U)
    {
        response->result = APP_STORAGE_RESULT_INVALID_NAME;
        return;
    }

    if (request->operation == APP_STORAGE_OP_WRITE_LOG)
    {
        temporary_path = STORAGE_LOG_TEMP_PATH;
        backup_path = STORAGE_LOG_BACKUP_PATH;
    }
    else if (request->operation == APP_STORAGE_OP_WRITE_SETTINGS)
    {
        temporary_path = STORAGE_SETTINGS_TEMP_PATH;
        backup_path = STORAGE_SETTINGS_BACKUP_PATH;
    }
    else if (request->operation == APP_STORAGE_OP_WRITE_FAULT)
    {
        temporary_path = STORAGE_FAULT_TEMP_PATH;
        backup_path = STORAGE_FAULT_BACKUP_PATH;
    }
    else
    {
        temporary_path = STORAGE_BINARY_TEMP_PATH;
        backup_path = STORAGE_BINARY_BACKUP_PATH;
    }

    app_storage_make_path(path, request->name);
    (void)app_storage_recover_binary_target(path, temporary_path, backup_path);
    (void)f_unlink(temporary_path);

    result = f_open(&file, temporary_path,
                    FA_CREATE_ALWAYS | FA_WRITE);
    transferred = 0U;
    if (result == FR_OK)
    {
        result = f_write(&file, request->binary_data,
                         request->binary_length, &transferred);
        if (result == FR_OK && transferred == request->binary_length)
        {
            result = f_sync(&file);
        }
        else if (result == FR_OK)
        {
            result = FR_DISK_ERR;
        }
        f_close(&file);
    }
    if (result != FR_OK)
    {
        (void)f_unlink(temporary_path);
        response->filesystem_result = (uint8_t)result;
        response->result = app_storage_map_result(result);
        response->data_length = (uint32_t)transferred;
        return;
    }

    had_old_file = (f_stat(path, &information) == FR_OK) ? 1U : 0U;
    if (had_old_file)
    {
        (void)f_unlink(backup_path);
        result = f_rename(path, backup_path);
    }
    if (result == FR_OK)
    {
        result = f_rename(temporary_path, path);
    }
    if (result != FR_OK)
    {
        if (had_old_file)
        {
            old_result = f_rename(backup_path, path);
            (void)old_result;
        }
        (void)f_unlink(temporary_path);
    }
    else if (had_old_file)
    {
        (void)f_unlink(backup_path);
    }

    response->filesystem_result = (uint8_t)result;
    response->result = app_storage_map_result(result);
    response->data_length = (result == FR_OK) ? request->binary_length : 0U;
}

static void app_storage_draw_list(app_storage_binary_response_t *response)
{
    DIR directory;
    FILINFO information;
    FRESULT result;
    uint8_t count;

    count = 0U;
    result = f_opendir(&directory, "0:/");
    if (result == FR_OK)
    {
        for (;;)
        {
            result = f_readdir(&directory, &information);
            if (result != FR_OK || information.fname[0] == '\0') break;
            if ((information.fattrib & AM_DIR) == 0U &&
                app_storage_is_draw_name(information.fname) &&
                count < 8U)
            {
                app_storage_copy_text(response->draw_names[count],
                                       sizeof(response->draw_names[count]),
                                       information.fname);
                count++;
            }
        }
        f_closedir(&directory);
    }
    response->filesystem_result = (uint8_t)result;
    response->result = app_storage_map_result(result);
    response->draw_count = count;
}

static void app_storage_draw_rename(const app_storage_request_t *request,
                                    app_storage_binary_response_t *response)
{
    FILINFO information;
    FRESULT result;
    char source_path[APP_STORAGE_PATH_LENGTH];
    char target_path[APP_STORAGE_PATH_LENGTH];

    if (!app_storage_name_is_valid(request->name) ||
        !app_storage_name_is_valid(request->new_name) ||
        !app_storage_is_draw_name(request->name) ||
        !app_storage_is_draw_name(request->new_name))
    {
        response->result = APP_STORAGE_RESULT_INVALID_NAME;
        return;
    }
    if (strcmp(request->name, request->new_name) == 0)
    {
        response->result = APP_STORAGE_RESULT_OK;
        return;
    }
    app_storage_make_path(source_path, request->name);
    app_storage_make_path(target_path, request->new_name);
    result = f_stat(source_path, &information);
    if (result != FR_OK)
    {
        response->filesystem_result = (uint8_t)result;
        response->result = app_storage_map_result(result);
        return;
    }
    result = f_stat(target_path, &information);
    if (result == FR_OK)
    {
        response->filesystem_result = (uint8_t)FR_EXIST;
        response->result = APP_STORAGE_RESULT_EXISTS;
        return;
    }
    if (result != FR_NO_FILE && result != FR_NO_PATH)
    {
        response->filesystem_result = (uint8_t)result;
        response->result = app_storage_map_result(result);
        return;
    }
    result = f_rename(source_path, target_path);
    response->filesystem_result = (uint8_t)result;
    response->result = app_storage_map_result(result);
}

/**
 * @brief app_storage_process_binary：解析并处理当前事件或数据，根据结果推进模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param request 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param response 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void app_storage_process_binary(const app_storage_request_t *request,
                                        app_storage_binary_response_t *response)
{
    FRESULT mount_result;

    memset(response, 0, sizeof(*response));
    response->request_id = request->request_id;
    response->operation = request->operation;
    if (g_storage_state != APP_STORAGE_STATE_READY)
    {
        mount_result = app_storage_mount();
        if (mount_result != FR_OK)
        {
            response->filesystem_result = (uint8_t)mount_result;
            response->result = APP_STORAGE_RESULT_NOT_READY;
            return;
        }
    }

    if (request->operation == APP_STORAGE_OP_DRAW_LIST)
    {
        app_storage_draw_list(response);
    }
    else if (request->operation == APP_STORAGE_OP_DRAW_RENAME)
    {
        app_storage_draw_rename(request, response);
    }
    else if (request->operation == APP_STORAGE_OP_READ_BINARY ||
             request->operation == APP_STORAGE_OP_READ_SETTINGS)
    {
        app_storage_read_binary(request, response);
    }
    else
    {
        app_storage_write_binary(request, response);
    }
}

/**
 * @brief app_storage_process：解析并处理当前事件或数据，根据结果推进模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param request 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param response 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void app_storage_process(const app_storage_request_t *request,
                                app_storage_response_t *response)
{
    FRESULT mount_result;

    memset(response, 0, sizeof(*response));
    response->request_id = request->request_id;
    response->operation = request->operation;

    if (g_storage_state != APP_STORAGE_STATE_READY)
    {
        mount_result = app_storage_mount();
        if (mount_result != FR_OK)
        {
            response->filesystem_result = (uint8_t)mount_result;
            response->result = APP_STORAGE_RESULT_NOT_READY;
            return;
        }
    }

    switch (request->operation)
    {
        case APP_STORAGE_OP_LIST:
            app_storage_list(response);
            break;

        case APP_STORAGE_OP_CREATE:
            app_storage_create(request, response);
            break;

        case APP_STORAGE_OP_READ:
            app_storage_read(request, response);
            break;

        case APP_STORAGE_OP_WRITE:
            app_storage_write(request, response);
            break;

        case APP_STORAGE_OP_DELETE:
            app_storage_delete(request, response);
            break;

        case APP_STORAGE_OP_RENAME:
            app_storage_rename(request, response);
            break;

        default:
            response->result = APP_STORAGE_RESULT_IO_ERROR;
            break;
    }
}

/**
 * @brief app_storage_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
BaseType_t app_storage_init(void)
{
    g_request_queue = xQueueCreate(STORAGE_REQUEST_QUEUE_LENGTH,
                                   sizeof(app_storage_request_t));
    g_response_queue = xQueueCreate(STORAGE_RESPONSE_QUEUE_LENGTH,
                                    sizeof(app_storage_response_t));
    g_binary_response_queue = xQueueCreate(STORAGE_BINARY_QUEUE_LENGTH,
                                    sizeof(app_storage_binary_response_t));
    g_log_response_queue = xQueueCreate(STORAGE_LOG_QUEUE_LENGTH,
                                    sizeof(app_storage_binary_response_t));
    g_settings_response_queue = xQueueCreate(STORAGE_SETTINGS_QUEUE_LENGTH,
                                     sizeof(app_storage_binary_response_t));
    g_audio_response_queue = xQueueCreate(STORAGE_AUDIO_QUEUE_LENGTH,
                                     sizeof(app_storage_audio_response_t));
    g_fault_response_queue = xQueueCreate(STORAGE_FAULT_QUEUE_LENGTH,
                                     sizeof(app_storage_binary_response_t));
    if (g_request_queue == NULL || g_response_queue == NULL ||
        g_binary_response_queue == NULL || g_log_response_queue == NULL ||
        g_settings_response_queue == NULL || g_audio_response_queue == NULL ||
        g_fault_response_queue == NULL)
    {
        return pdFAIL;
    }

    vQueueAddToRegistry(g_request_queue, "StorageRequests");
    vQueueAddToRegistry(g_response_queue, "StorageResponses");
    vQueueAddToRegistry(g_binary_response_queue, "StorageBinaryResponses");
    vQueueAddToRegistry(g_log_response_queue, "StorageLogResponses");
    vQueueAddToRegistry(g_settings_response_queue, "StorageSettingsResponses");
    vQueueAddToRegistry(g_audio_response_queue, "StorageAudioResponses");
    vQueueAddToRegistry(g_fault_response_queue, "StorageFaultResponses");
    return pdPASS;
}

/**
 * @brief app_storage_submit：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param request 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
BaseType_t app_storage_submit(const app_storage_request_t *request)
{
    BaseType_t result;
    UBaseType_t depth;

    if (request == NULL || g_request_queue == NULL)
    {
        return pdFAIL;
    }
    result = xQueueSend(g_request_queue, request, 0U);
    if (result != pdPASS)
    {
        g_request_queue_full_count++;
        return pdFAIL;
    }
    depth = uxQueueMessagesWaiting(g_request_queue);
    if (depth > g_request_queue_peak)
    {
        g_request_queue_peak = depth;
    }
    return pdPASS;
}

/**
 * @brief app_storage_receive：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param response 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
BaseType_t app_storage_receive(app_storage_response_t *response)
{
    if (response == NULL || g_response_queue == NULL)
    {
        return pdFAIL;
    }
    return xQueueReceive(g_response_queue, response, 0U);
}

/**
 * @brief app_storage_receive_binary：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param response 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
BaseType_t app_storage_receive_binary(app_storage_binary_response_t *response)
{
    if (response == NULL || g_binary_response_queue == NULL)
    {
        return pdFAIL;
    }
    return xQueueReceive(g_binary_response_queue, response, 0U);
}

/**
 * @brief app_storage_receive_log：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param response 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
BaseType_t app_storage_receive_log(app_storage_binary_response_t *response)
{
    if (response == NULL || g_log_response_queue == NULL)
    {
        return pdFAIL;
    }
    return xQueueReceive(g_log_response_queue, response, 0U);
}

/**
 * @brief app_storage_receive_settings：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param response 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
BaseType_t app_storage_receive_settings(app_storage_binary_response_t *response)
{
    if (response == NULL || g_settings_response_queue == NULL)
    {
        return pdFAIL;
    }
    return xQueueReceive(g_settings_response_queue, response, 0U);
}

/**
 * @brief app_storage_receive_audio：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param response 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
BaseType_t app_storage_receive_audio(app_storage_audio_response_t *response)
{
    if (response == NULL || g_audio_response_queue == NULL)
    {
        return pdFAIL;
    }
    return xQueueReceive(g_audio_response_queue, response, 0U);
}

/**
 * @brief app_storage_receive_fault：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param response 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
BaseType_t app_storage_receive_fault(app_storage_binary_response_t *response)
{
    if (response == NULL || g_fault_response_queue == NULL)
    {
        return pdFAIL;
    }
    return xQueueReceive(g_fault_response_queue, response, 0U);
}

/**
 * @brief app_storage_get_stats：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param stats 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_storage_get_stats(app_storage_stats_t *stats)
{
    if (stats != NULL)
    {
        stats->request_queue_peak = g_request_queue_peak;
        stats->request_queue_full_count = g_request_queue_full_count;
        stats->response_drop_count = g_response_drop_count;
        stats->filesystem_error_count = g_filesystem_error_count;
        stats->request_queue_depth = (g_request_queue != NULL) ?
            (uint16_t)uxQueueMessagesWaiting(g_request_queue) : 0U;
    }
}

/**
 * @brief app_storage_get_state：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
app_storage_state_t app_storage_get_state(void)
{
    return g_storage_state;
}

/**
 * @brief app_storage_get_capacity_mb：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint32_t app_storage_get_capacity_mb(void)
{
    return g_capacity_mb;
}

/**
 * @brief AppStorageTask：作为 FreeRTOS 任务入口，循环处理事件、周期工作和运行状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param argument 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 * @warning 该入口具有特定中断或任务上下文，禁止执行不符合该上下文约束的操作。
 */
void AppStorageTask(void *argument)
{
    static app_storage_response_t response;
    app_storage_binary_response_t binary_response;
    app_storage_audio_response_t audio_response;
    app_storage_request_t request;
    FRESULT result;

    (void)argument;
    printf("SD storage service: starting\r\n");
    result = app_storage_mount();
    if (result == FR_OK)
    {
        app_logs_add(APP_LOG_LEVEL_INFO, "STORAGE", "SD FAT32 MOUNTED");
        printf("SD card: %lu MB, FAT32 mount: PASS\r\n",
               (unsigned long)g_capacity_mb);
        app_storage_run_self_test();
    }
    else
    {
        app_logs_add(APP_LOG_LEVEL_ERROR, "STORAGE", "SD MOUNT FAILED");
        printf("SD/FatFs mount: FAIL (%u)\r\n", (unsigned int)result);
    }

    while (1)
    {
        app_health_beat(APP_HEALTH_STORAGE);
        if (xQueueReceive(g_request_queue, &request,
                          pdMS_TO_TICKS(500U)) == pdPASS)
        {
            if (request.operation == APP_STORAGE_OP_AUDIO_SCAN ||
                request.operation == APP_STORAGE_OP_AUDIO_OPEN ||
                request.operation == APP_STORAGE_OP_AUDIO_READ ||
                request.operation == APP_STORAGE_OP_AUDIO_SEEK ||
                request.operation == APP_STORAGE_OP_AUDIO_CLOSE)
            {
                app_storage_process_audio(&request, &audio_response);
                if (audio_response.result != APP_STORAGE_RESULT_OK)
                {
                    g_filesystem_error_count++;
                }
                if (xQueueSend(g_audio_response_queue, &audio_response,
                    pdMS_TO_TICKS(STORAGE_RESPONSE_TIMEOUT_MS)) != pdPASS)
                {
                    g_response_drop_count++;
                }
            }
            else if (request.operation == APP_STORAGE_OP_READ_BINARY ||
                request.operation == APP_STORAGE_OP_WRITE_BINARY ||
                request.operation == APP_STORAGE_OP_DRAW_LIST ||
                request.operation == APP_STORAGE_OP_DRAW_RENAME)
            {
                app_storage_process_binary(&request, &binary_response);
                if (binary_response.result != APP_STORAGE_RESULT_OK)
                {
                    g_filesystem_error_count++;
                }
                if (xQueueSend(g_binary_response_queue, &binary_response,
                    pdMS_TO_TICKS(STORAGE_RESPONSE_TIMEOUT_MS)) != pdPASS)
                {
                    g_response_drop_count++;
                }
            }
            else if (request.operation == APP_STORAGE_OP_WRITE_LOG)
            {
                app_storage_process_binary(&request, &binary_response);
                if (binary_response.result != APP_STORAGE_RESULT_OK)
                {
                    g_filesystem_error_count++;
                }
                if (xQueueSend(g_log_response_queue, &binary_response,
                    pdMS_TO_TICKS(STORAGE_RESPONSE_TIMEOUT_MS)) != pdPASS)
                {
                    g_response_drop_count++;
                }
            }
            else if (request.operation == APP_STORAGE_OP_READ_SETTINGS ||
                     request.operation == APP_STORAGE_OP_WRITE_SETTINGS)
            {
                app_storage_process_binary(&request, &binary_response);
                if (binary_response.result != APP_STORAGE_RESULT_OK)
                {
                    g_filesystem_error_count++;
                }
                if (xQueueSend(g_settings_response_queue, &binary_response,
                    pdMS_TO_TICKS(STORAGE_RESPONSE_TIMEOUT_MS)) != pdPASS)
                {
                    g_response_drop_count++;
                }
            }
            else if (request.operation == APP_STORAGE_OP_WRITE_FAULT)
            {
                app_storage_process_binary(&request, &binary_response);
                if (binary_response.result != APP_STORAGE_RESULT_OK)
                {
                    g_filesystem_error_count++;
                }
                if (xQueueSend(g_fault_response_queue, &binary_response,
                    pdMS_TO_TICKS(STORAGE_RESPONSE_TIMEOUT_MS)) != pdPASS)
                {
                    g_response_drop_count++;
                }
            }
            else
            {
                app_storage_process(&request, &response);
                if (response.result != APP_STORAGE_RESULT_OK)
                {
                    g_filesystem_error_count++;
                }
                if (xQueueSend(g_response_queue, &response,
                    pdMS_TO_TICKS(STORAGE_RESPONSE_TIMEOUT_MS)) != pdPASS)
                {
                    g_response_drop_count++;
                }
            }
        }
    }
}
