/**
 * @file app_settings.c
 * @brief 维护亮度、音量等用户设置并处理设置界面交互。
 * @details 这是 app_settings 模块的实现文件（App/app_settings.c）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#include "app_settings.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "app_audio.h"
#include "app_input.h"
#include "app_logs.h"
#include "app_rtc.h"
#include "app_screen.h"
#include "app_storage.h"
#include "app_ui.h"
#include "task.h"

/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define APP_SETTINGS_FILE_NAME       "SETTINGS.CFG"
#define APP_SETTINGS_FILE_VERSION      3U
#define APP_SETTINGS_LEGACY_VERSION    1U
#define APP_SETTINGS_PREVIOUS_VERSION  2U

/** @brief 模块数据类型：描述本模块维护的状态、配置或数据快照。 */
typedef struct
{
    uint32_t idle_timeout_seconds;
    uint8_t serial_output_enabled;
    uint8_t cursor_sensitivity;
    uint8_t cursor_size;
    uint8_t brightness_percent;
    uint8_t volume_percent;
    uint8_t reserved[3];
} app_settings_config_t;

typedef struct
{
    uint32_t idle_timeout_seconds;
    uint8_t serial_output_enabled;
    uint8_t cursor_sensitivity;
    uint8_t cursor_size;
    uint8_t brightness_percent;
} app_settings_legacy_config_t;

typedef struct
{
    uint8_t magic[4];
    uint16_t version;
    uint16_t payload_size;
    app_settings_config_t config;
    uint32_t payload_crc;
} app_settings_document_t;

typedef struct
{
    uint8_t magic[4];
    uint16_t version;
    uint16_t payload_size;
    app_settings_legacy_config_t config;
    uint32_t payload_crc;
} app_settings_legacy_document_t;

typedef struct
{
    uint8_t initialized;
    uint8_t active;
    uint8_t busy;
    uint8_t dirty;
    uint8_t save_pending;
    uint8_t pending_volume_valid;
    uint8_t pending_volume;
    uint8_t rtc_editing;
    TickType_t save_retry_tick;
    char status[64];
    app_rtc_datetime_t rtc_datetime;
    app_settings_config_t config;
    app_settings_document_t io_document;
} app_settings_state_t;

static app_settings_state_t g_settings;

/**
 * @brief app_settings_rtc_days_in_month：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param year 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param month 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint8_t app_settings_rtc_days_in_month(uint16_t year, uint8_t month)
{
    static const uint8_t days[12] =
    {
        31U, 28U, 31U, 30U, 31U, 30U,
        31U, 31U, 30U, 31U, 30U, 31U
    };

    if (month < 1U || month > 12U)
    {
        return 31U;
    }
    if (month == 2U && (year % 4U) == 0U)
    {
        return 29U;
    }
    return days[month - 1U];
}

/**
 * @brief app_settings_rtc_clamp_date：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
static void app_settings_rtc_clamp_date(void)
{
    uint8_t maximum_date;

    maximum_date = app_settings_rtc_days_in_month(
        g_settings.rtc_datetime.year, g_settings.rtc_datetime.month);
    if (g_settings.rtc_datetime.date > maximum_date)
    {
        g_settings.rtc_datetime.date = maximum_date;
    }
}

/**
 * @brief app_settings_copy_text：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param destination 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param destination_size 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param source 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void app_settings_copy_text(char *destination,
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

/**
 * @brief app_settings_set_status：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param status 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void app_settings_set_status(const char *status)
{
    app_settings_copy_text(g_settings.status,
                           sizeof(g_settings.status), status);
}

/**
 * @brief app_settings_default_config：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static app_settings_config_t app_settings_default_config(void)
{
    app_settings_config_t config;

    memset(&config, 0, sizeof(config));
    config.idle_timeout_seconds = APP_SETTINGS_SCREEN_OFF_60_SECONDS;
    config.serial_output_enabled = 1U;
    config.cursor_sensitivity = APP_INPUT_SENSITIVITY_NORMAL;
    config.cursor_size = APP_UI_CURSOR_SIZE_MEDIUM;
    config.brightness_percent = 100U;
    config.volume_percent = 50U;
    return config;
}

/**
 * @brief app_settings_apply_config：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param config 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void app_settings_apply_config(const app_settings_config_t *config)
{
    app_input_set_cursor_sensitivity(config->cursor_sensitivity);
    app_ui_set_cursor_size(config->cursor_size);
    app_screen_set_brightness(config->brightness_percent);
    app_audio_set_volume(config->volume_percent);
}

/**
 * @brief app_settings_set_config：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param config 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void app_settings_set_config(const app_settings_config_t *config)
{
    taskENTER_CRITICAL();
    g_settings.config = *config;
    taskEXIT_CRITICAL();
    app_settings_apply_config(config);
}

/**
 * @brief app_settings_get_config：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static app_settings_config_t app_settings_get_config(void)
{
    app_settings_config_t config;

    taskENTER_CRITICAL();
    config = g_settings.config;
    taskEXIT_CRITICAL();
    return config;
}

/**
 * @brief app_settings_timeout_is_valid：检查函数名所描述的条件是否成立，并返回明确的判断结果。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param timeout_seconds 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint8_t app_settings_timeout_is_valid(uint32_t timeout_seconds)
{
    return (timeout_seconds == APP_SETTINGS_SCREEN_OFF_DISABLED ||
            timeout_seconds == APP_SETTINGS_SCREEN_OFF_30_SECONDS ||
            timeout_seconds == APP_SETTINGS_SCREEN_OFF_60_SECONDS ||
            timeout_seconds == APP_SETTINGS_SCREEN_OFF_120_SECONDS) ? 1U : 0U;
}

/**
 * @brief app_settings_brightness_is_valid：检查函数名所描述的条件是否成立，并返回明确的判断结果。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param brightness_percent 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint8_t app_settings_brightness_is_valid(uint8_t brightness_percent)
{
    return (brightness_percent == 25U || brightness_percent == 50U ||
            brightness_percent == 75U || brightness_percent == 100U) ? 1U : 0U;
}

/**
 * @brief app_settings_volume_is_valid：检查函数名所描述的条件是否成立，并返回明确的判断结果。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param volume_percent 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint8_t app_settings_volume_is_valid(uint8_t volume_percent)
{
    return (volume_percent == 0U || volume_percent == 25U ||
            volume_percent == 50U || volume_percent == 75U ||
            volume_percent == 100U) ? 1U : 0U;
}

/**
 * @brief app_settings_crc32：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param data 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param length 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint32_t app_settings_crc32(const void *data, uint32_t length)
{
    const uint8_t *bytes;
    uint32_t crc;
    uint32_t index;
    uint8_t bit;

    bytes = (const uint8_t *)data;
    crc = 0xFFFFFFFFU;
    for (index = 0U; index < length; index++)
    {
        crc ^= bytes[index];
        for (bit = 0U; bit < 8U; bit++)
        {
            if ((crc & 1U) != 0U)
            {
                crc = (crc >> 1U) ^ 0xEDB88320U;
            }
            else
            {
                crc >>= 1U;
            }
        }
    }
    return crc ^ 0xFFFFFFFFU;
}

/**
 * @brief app_settings_document_is_valid：检查函数名所描述的条件是否成立，并返回明确的判断结果。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param data_length 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint8_t app_settings_document_is_valid(uint32_t data_length)
{
    const app_settings_document_t *document;
    const app_settings_legacy_document_t *legacy;

    document = &g_settings.io_document;
    legacy = (const app_settings_legacy_document_t *)&g_settings.io_document;
    if (sizeof(app_settings_config_t) != 12U ||
        sizeof(app_settings_document_t) != 24U ||
        sizeof(app_settings_legacy_config_t) != 8U ||
        sizeof(app_settings_legacy_document_t) != 20U ||
        document->magic[0] != 'C' || document->magic[1] != 'F' ||
        document->magic[2] != 'G' || document->magic[3] != '1' ||
        (document->version != APP_SETTINGS_FILE_VERSION &&
         document->version != APP_SETTINGS_PREVIOUS_VERSION &&
         document->version != APP_SETTINGS_LEGACY_VERSION))
    {
        return 0U;
    }

    if (document->version == APP_SETTINGS_LEGACY_VERSION ||
        document->version == APP_SETTINGS_PREVIOUS_VERSION)
    {
        if (data_length != sizeof(app_settings_legacy_document_t) ||
            legacy->payload_size != sizeof(app_settings_legacy_config_t) ||
            !app_settings_timeout_is_valid(
                legacy->config.idle_timeout_seconds) ||
            legacy->config.serial_output_enabled > 1U)
        {
            return 0U;
        }
        if (document->version == APP_SETTINGS_LEGACY_VERSION)
        {
            if (legacy->config.cursor_sensitivity != 0U ||
                legacy->config.cursor_size != 0U ||
                legacy->config.brightness_percent != 0U)
            {
                return 0U;
            }
        }
        else if (legacy->config.cursor_sensitivity < APP_INPUT_SENSITIVITY_LOW ||
                 legacy->config.cursor_sensitivity > APP_INPUT_SENSITIVITY_HIGH ||
                 legacy->config.cursor_size < APP_UI_CURSOR_SIZE_SMALL ||
                 legacy->config.cursor_size > APP_UI_CURSOR_SIZE_LARGE ||
                 !app_settings_brightness_is_valid(
                     legacy->config.brightness_percent))
        {
            return 0U;
        }
        return (app_settings_crc32(&legacy->config,
                                   sizeof(legacy->config)) ==
                legacy->payload_crc) ? 1U : 0U;
    }

    if (data_length != sizeof(app_settings_document_t) ||
        document->payload_size != sizeof(app_settings_config_t) ||
        !app_settings_timeout_is_valid(document->config.idle_timeout_seconds) ||
        document->config.serial_output_enabled > 1U ||
        document->config.cursor_sensitivity < APP_INPUT_SENSITIVITY_LOW ||
        document->config.cursor_sensitivity > APP_INPUT_SENSITIVITY_HIGH ||
        document->config.cursor_size < APP_UI_CURSOR_SIZE_SMALL ||
        document->config.cursor_size > APP_UI_CURSOR_SIZE_LARGE ||
        !app_settings_brightness_is_valid(document->config.brightness_percent) ||
        !app_settings_volume_is_valid(document->config.volume_percent) ||
        document->config.reserved[0] != 0U ||
        document->config.reserved[1] != 0U ||
        document->config.reserved[2] != 0U)
    {
        return 0U;
    }

    return (app_settings_crc32(&document->config,
                               sizeof(document->config)) ==
            document->payload_crc) ? 1U : 0U;
}

/**
 * @brief app_settings_redraw：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
static void app_settings_redraw(void)
{
    app_settings_config_t config;

    if (!g_settings.active)
    {
        return;
    }
    if (g_settings.rtc_editing)
    {
        app_ui_show_time_settings(&g_settings.rtc_datetime,
                                  app_rtc_is_available());
        return;
    }
    config = app_settings_get_config();
    app_ui_show_settings(config.cursor_sensitivity,
                         config.cursor_size,
                         config.brightness_percent,
                         config.volume_percent,
                         config.idle_timeout_seconds,
                         config.serial_output_enabled,
                         g_settings.dirty,
                         g_settings.status,
                         g_settings.busy);
}

/**
 * @brief app_settings_submit_read：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static BaseType_t app_settings_submit_read(void)
{
    app_storage_request_t request;

    memset(&request, 0, sizeof(request));
    request.operation = APP_STORAGE_OP_READ_SETTINGS;
    app_settings_copy_text(request.name, sizeof(request.name),
                           APP_SETTINGS_FILE_NAME);
    request.binary_data = &g_settings.io_document;
    request.binary_capacity = sizeof(g_settings.io_document);
    if (app_storage_submit(&request) != pdPASS)
    {
        app_settings_set_status("SETTINGS LOAD QUEUE IS FULL");
        app_logs_add(APP_LOG_LEVEL_WARNING, "SETTINGS", "LOAD QUEUE FULL");
        return pdFAIL;
    }
    g_settings.busy = 1U;
    app_settings_set_status("LOADING SETTINGS.CFG");
    return pdPASS;
}

/**
 * @brief app_settings_save：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
static void app_settings_save(void)
{
    app_storage_request_t request;
    app_settings_config_t config;

    if (g_settings.busy)
    {
        g_settings.save_pending = 1U;
        return;
    }
    config = app_settings_get_config();
    g_settings.io_document.magic[0] = 'C';
    g_settings.io_document.magic[1] = 'F';
    g_settings.io_document.magic[2] = 'G';
    g_settings.io_document.magic[3] = '1';
    g_settings.io_document.version = APP_SETTINGS_FILE_VERSION;
    g_settings.io_document.payload_size = sizeof(app_settings_config_t);
    g_settings.io_document.config = config;
    g_settings.io_document.payload_crc =
        app_settings_crc32(&g_settings.io_document.config,
                           sizeof(g_settings.io_document.config));

    memset(&request, 0, sizeof(request));
    request.operation = APP_STORAGE_OP_WRITE_SETTINGS;
    app_settings_copy_text(request.name, sizeof(request.name),
                           APP_SETTINGS_FILE_NAME);
    request.binary_data = &g_settings.io_document;
    request.binary_length = sizeof(g_settings.io_document);
    if (app_storage_submit(&request) != pdPASS)
    {
        g_settings.save_pending = 1U;
        g_settings.save_retry_tick = xTaskGetTickCount();
        app_settings_set_status("SETTINGS SAVE QUEUE IS FULL");
        app_logs_add(APP_LOG_LEVEL_WARNING, "SETTINGS", "SAVE QUEUE FULL");
        app_settings_redraw();
        return;
    }

    g_settings.busy = 1U;
    g_settings.save_pending = 0U;
    app_settings_set_status("SAVING SETTINGS.CFG");
    app_settings_redraw();
}

/**
 * @brief app_settings_change：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param config 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param status 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void app_settings_change(const app_settings_config_t *config,
                                const char *status)
{
    app_settings_set_config(config);
    g_settings.dirty = 1U;
    app_settings_set_status(status);
    app_settings_save();
}

/**
 * @brief app_settings_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_settings_init(void)
{
    app_settings_config_t defaults;

    if (g_settings.initialized)
    {
        return;
    }
    memset(&g_settings, 0, sizeof(g_settings));
    defaults = app_settings_default_config();
    app_settings_set_config(&defaults);
    g_settings.initialized = 1U;
    g_settings.dirty = 1U;
    (void)app_settings_submit_read();
}

/**
 * @brief app_settings_open：启动或启用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_settings_open(void)
{
    app_settings_init();
    g_settings.active = 1U;
    app_settings_redraw();
}

/**
 * @brief app_settings_close：停止或禁用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_settings_close(void)
{
    g_settings.active = 0U;
    g_settings.rtc_editing = 0U;
}

/**
 * @brief app_settings_handle_time_event：解析并处理当前事件或数据，根据结果推进模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param event 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void app_settings_handle_time_event(const app_input_event_t *event)
{
    app_ui_time_action_t action;
    uint8_t maximum_date;

    action = app_ui_time_action_at(event->x, event->y);
    if (action == APP_UI_TIME_ACTION_CANCEL)
    {
        g_settings.rtc_editing = 0U;
        app_settings_set_status("TIME CHANGE CANCELLED");
        app_settings_redraw();
        return;
    }
    if (action == APP_UI_TIME_ACTION_APPLY)
    {
        if (app_rtc_set_datetime(&g_settings.rtc_datetime))
        {
            g_settings.rtc_editing = 0U;
            app_settings_set_status("DS3231 DATE AND TIME UPDATED");
            app_logs_add(APP_LOG_LEVEL_INFO, "RTC", "DATE AND TIME UPDATED");
        }
        else
        {
            app_settings_set_status("DS3231 TIME UPDATE FAILED");
            app_logs_add(APP_LOG_LEVEL_ERROR, "RTC", "TIME UPDATE FAILED");
        }
        app_settings_redraw();
        return;
    }

    if (action == APP_UI_TIME_ACTION_YEAR_DOWN)
    {
        g_settings.rtc_datetime.year =
            g_settings.rtc_datetime.year > 2000U ?
            (uint16_t)(g_settings.rtc_datetime.year - 1U) : 2099U;
        app_settings_rtc_clamp_date();
    }
    else if (action == APP_UI_TIME_ACTION_YEAR_UP)
    {
        g_settings.rtc_datetime.year =
            g_settings.rtc_datetime.year < 2099U ?
            (uint16_t)(g_settings.rtc_datetime.year + 1U) : 2000U;
        app_settings_rtc_clamp_date();
    }
    else if (action == APP_UI_TIME_ACTION_MONTH_DOWN)
    {
        g_settings.rtc_datetime.month =
            g_settings.rtc_datetime.month > 1U ?
            (uint8_t)(g_settings.rtc_datetime.month - 1U) : 12U;
        app_settings_rtc_clamp_date();
    }
    else if (action == APP_UI_TIME_ACTION_MONTH_UP)
    {
        g_settings.rtc_datetime.month =
            g_settings.rtc_datetime.month < 12U ?
            (uint8_t)(g_settings.rtc_datetime.month + 1U) : 1U;
        app_settings_rtc_clamp_date();
    }
    else if (action == APP_UI_TIME_ACTION_DATE_DOWN)
    {
        maximum_date = app_settings_rtc_days_in_month(
            g_settings.rtc_datetime.year, g_settings.rtc_datetime.month);
        g_settings.rtc_datetime.date =
            g_settings.rtc_datetime.date > 1U ?
            (uint8_t)(g_settings.rtc_datetime.date - 1U) : maximum_date;
    }
    else if (action == APP_UI_TIME_ACTION_DATE_UP)
    {
        maximum_date = app_settings_rtc_days_in_month(
            g_settings.rtc_datetime.year, g_settings.rtc_datetime.month);
        g_settings.rtc_datetime.date =
            g_settings.rtc_datetime.date < maximum_date ?
            (uint8_t)(g_settings.rtc_datetime.date + 1U) : 1U;
    }
    else if (action == APP_UI_TIME_ACTION_HOUR_DOWN)
    {
        g_settings.rtc_datetime.hour =
            g_settings.rtc_datetime.hour > 0U ?
            (uint8_t)(g_settings.rtc_datetime.hour - 1U) : 23U;
    }
    else if (action == APP_UI_TIME_ACTION_HOUR_UP)
    {
        g_settings.rtc_datetime.hour =
            g_settings.rtc_datetime.hour < 23U ?
            (uint8_t)(g_settings.rtc_datetime.hour + 1U) : 0U;
    }
    else if (action == APP_UI_TIME_ACTION_MINUTE_DOWN)
    {
        g_settings.rtc_datetime.minute =
            g_settings.rtc_datetime.minute > 0U ?
            (uint8_t)(g_settings.rtc_datetime.minute - 1U) : 59U;
    }
    else if (action == APP_UI_TIME_ACTION_MINUTE_UP)
    {
        g_settings.rtc_datetime.minute =
            g_settings.rtc_datetime.minute < 59U ?
            (uint8_t)(g_settings.rtc_datetime.minute + 1U) : 0U;
    }
    else if (action == APP_UI_TIME_ACTION_SECOND_DOWN)
    {
        g_settings.rtc_datetime.second =
            g_settings.rtc_datetime.second > 0U ?
            (uint8_t)(g_settings.rtc_datetime.second - 1U) : 59U;
    }
    else if (action == APP_UI_TIME_ACTION_SECOND_UP)
    {
        g_settings.rtc_datetime.second =
            g_settings.rtc_datetime.second < 59U ?
            (uint8_t)(g_settings.rtc_datetime.second + 1U) : 0U;
    }
    else
    {
        return;
    }
    app_settings_redraw();
}

/**
 * @brief app_settings_handle_event：解析并处理当前事件或数据，根据结果推进模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param event 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_settings_handle_event(const app_input_event_t *event)
{
    app_ui_settings_action_t action;
    app_settings_config_t config;

    if (!g_settings.active || event == NULL ||
        event->type != APP_INPUT_EVENT_DOWN || g_settings.busy)
    {
        return;
    }

    if (g_settings.rtc_editing)
    {
        app_settings_handle_time_event(event);
        return;
    }

    action = app_ui_settings_action_at(event->x, event->y);
    config = app_settings_get_config();
    if (action == APP_UI_SETTINGS_ACTION_SENSITIVITY_LOW)
    {
        config.cursor_sensitivity = APP_INPUT_SENSITIVITY_LOW;
        app_settings_change(&config, "CURSOR SENSITIVITY LOW - AUTO SAVE");
    }
    else if (action == APP_UI_SETTINGS_ACTION_SENSITIVITY_NORMAL)
    {
        config.cursor_sensitivity = APP_INPUT_SENSITIVITY_NORMAL;
        app_settings_change(&config, "CURSOR SENSITIVITY NORMAL - AUTO SAVE");
    }
    else if (action == APP_UI_SETTINGS_ACTION_SENSITIVITY_HIGH)
    {
        config.cursor_sensitivity = APP_INPUT_SENSITIVITY_HIGH;
        app_settings_change(&config, "CURSOR SENSITIVITY HIGH - AUTO SAVE");
    }
    else if (action == APP_UI_SETTINGS_ACTION_CURSOR_SMALL)
    {
        config.cursor_size = APP_UI_CURSOR_SIZE_SMALL;
        app_settings_change(&config, "CURSOR SIZE SMALL - AUTO SAVE");
    }
    else if (action == APP_UI_SETTINGS_ACTION_CURSOR_MEDIUM)
    {
        config.cursor_size = APP_UI_CURSOR_SIZE_MEDIUM;
        app_settings_change(&config, "CURSOR SIZE MEDIUM - AUTO SAVE");
    }
    else if (action == APP_UI_SETTINGS_ACTION_CURSOR_LARGE)
    {
        config.cursor_size = APP_UI_CURSOR_SIZE_LARGE;
        app_settings_change(&config, "CURSOR SIZE LARGE - AUTO SAVE");
    }
    else if (action == APP_UI_SETTINGS_ACTION_BRIGHTNESS_25)
    {
        config.brightness_percent = 25U;
        app_settings_change(&config, "BRIGHTNESS 25 PERCENT - AUTO SAVE");
    }
    else if (action == APP_UI_SETTINGS_ACTION_BRIGHTNESS_50)
    {
        config.brightness_percent = 50U;
        app_settings_change(&config, "BRIGHTNESS 50 PERCENT - AUTO SAVE");
    }
    else if (action == APP_UI_SETTINGS_ACTION_BRIGHTNESS_75)
    {
        config.brightness_percent = 75U;
        app_settings_change(&config, "BRIGHTNESS 75 PERCENT - AUTO SAVE");
    }
    else if (action == APP_UI_SETTINGS_ACTION_BRIGHTNESS_100)
    {
        config.brightness_percent = 100U;
        app_settings_change(&config, "BRIGHTNESS 100 PERCENT - AUTO SAVE");
    }
    else if (action == APP_UI_SETTINGS_ACTION_VOLUME_0)
    {
        config.volume_percent = 0U;
        app_settings_change(&config, "VOLUME MUTED - AUTO SAVE");
    }
    else if (action == APP_UI_SETTINGS_ACTION_VOLUME_25)
    {
        config.volume_percent = 25U;
        app_settings_change(&config, "VOLUME 25 PERCENT - AUTO SAVE");
    }
    else if (action == APP_UI_SETTINGS_ACTION_VOLUME_50)
    {
        config.volume_percent = 50U;
        app_settings_change(&config, "VOLUME 50 PERCENT - AUTO SAVE");
    }
    else if (action == APP_UI_SETTINGS_ACTION_VOLUME_75)
    {
        config.volume_percent = 75U;
        app_settings_change(&config, "VOLUME 75 PERCENT - AUTO SAVE");
    }
    else if (action == APP_UI_SETTINGS_ACTION_VOLUME_100)
    {
        config.volume_percent = 100U;
        app_settings_change(&config, "VOLUME 100 PERCENT - AUTO SAVE");
    }
    else if (action == APP_UI_SETTINGS_ACTION_SCREEN_OFF_DISABLED)
    {
        config.idle_timeout_seconds = APP_SETTINGS_SCREEN_OFF_DISABLED;
        app_settings_change(&config, "SCREEN TIMEOUT DISABLED - AUTO SAVE");
    }
    else if (action == APP_UI_SETTINGS_ACTION_SCREEN_OFF_30)
    {
        config.idle_timeout_seconds = APP_SETTINGS_SCREEN_OFF_30_SECONDS;
        app_settings_change(&config, "SCREEN OFF 30 SECONDS - AUTO SAVE");
    }
    else if (action == APP_UI_SETTINGS_ACTION_SCREEN_OFF_60)
    {
        config.idle_timeout_seconds = APP_SETTINGS_SCREEN_OFF_60_SECONDS;
        app_settings_change(&config, "SCREEN OFF 60 SECONDS - AUTO SAVE");
    }
    else if (action == APP_UI_SETTINGS_ACTION_SCREEN_OFF_120)
    {
        config.idle_timeout_seconds = APP_SETTINGS_SCREEN_OFF_120_SECONDS;
        app_settings_change(&config, "SCREEN OFF 120 SECONDS - AUTO SAVE");
    }
    else if (action == APP_UI_SETTINGS_ACTION_SERIAL_ON)
    {
        config.serial_output_enabled = 1U;
        app_settings_change(&config, "SERIAL STATUS ENABLED - AUTO SAVE");
    }
    else if (action == APP_UI_SETTINGS_ACTION_SERIAL_OFF)
    {
        config.serial_output_enabled = 0U;
        app_settings_change(&config, "SERIAL STATUS DISABLED - AUTO SAVE");
    }
    else if (action == APP_UI_SETTINGS_ACTION_TIME)
    {
        if (app_rtc_get_datetime(&g_settings.rtc_datetime))
        {
            g_settings.rtc_editing = 1U;
            app_settings_redraw();
        }
        else
        {
            app_settings_set_status("DS3231 IS OFFLINE");
            app_logs_add(APP_LOG_LEVEL_ERROR, "RTC", "TIME SET UNAVAILABLE");
            app_settings_redraw();
        }
    }
    else if (action == APP_UI_SETTINGS_ACTION_DEFAULTS)
    {
        config = app_settings_default_config();
        app_settings_set_config(&config);
        g_settings.dirty = 1U;
        app_settings_set_status("DEFAULTS RESTORED - AUTO SAVE");
        app_logs_add(APP_LOG_LEVEL_WARNING, "SETTINGS", "DEFAULTS RESTORED");
        app_settings_save();
    }
    else if (action == APP_UI_SETTINGS_ACTION_SAVE && g_settings.dirty)
    {
        app_settings_save();
    }
}

/**
 * @brief app_settings_update：使用最新数据更新缓存、硬件输出或界面显示状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_settings_update(void)
{
    app_storage_binary_response_t response;
    app_settings_config_t defaults;
    const app_settings_legacy_document_t *legacy;

    while (app_storage_receive_settings(&response) == pdPASS)
    {
        g_settings.busy = 0U;
        if (response.operation == APP_STORAGE_OP_READ_SETTINGS)
        {
            if (response.result == APP_STORAGE_RESULT_OK &&
                app_settings_document_is_valid(response.data_length))
            {
                if (g_settings.io_document.version !=
                    APP_SETTINGS_FILE_VERSION)
                {
                    legacy = (const app_settings_legacy_document_t *)
                             &g_settings.io_document;
                    defaults = app_settings_default_config();
                    defaults.idle_timeout_seconds =
                        legacy->config.idle_timeout_seconds;
                    defaults.serial_output_enabled =
                        legacy->config.serial_output_enabled;
                    if (legacy->version == APP_SETTINGS_PREVIOUS_VERSION)
                    {
                        defaults.cursor_sensitivity =
                            legacy->config.cursor_sensitivity;
                        defaults.cursor_size = legacy->config.cursor_size;
                        defaults.brightness_percent =
                            legacy->config.brightness_percent;
                    }
                    app_settings_set_config(&defaults);
                    g_settings.dirty = 1U;
                    app_settings_set_status(
                        "OLD CONFIG UPGRADED - AUTO SAVE");
                    app_logs_add(APP_LOG_LEVEL_INFO, "SETTINGS",
                                 "OLD CONFIG MIGRATED TO VERSION 3");
                    app_settings_save();
                }
                else
                {
                    app_settings_set_config(&g_settings.io_document.config);
                    g_settings.dirty = 0U;
                    app_settings_set_status("SETTINGS.CFG LOADED");
                    app_logs_add(APP_LOG_LEVEL_INFO, "SETTINGS",
                                 "CONFIG LOADED");
                }
            }
            else
            {
                defaults = app_settings_default_config();
                app_settings_set_config(&defaults);
                g_settings.dirty = 1U;
                if (response.result == APP_STORAGE_RESULT_NOT_FOUND)
                {
                    app_settings_set_status("DEFAULT SETTINGS - TOUCH SAVE");
                    app_logs_add(APP_LOG_LEVEL_WARNING, "SETTINGS",
                                 "CONFIG FILE NOT FOUND");
                }
                else if (response.result == APP_STORAGE_RESULT_OK)
                {
                    app_settings_set_status("INVALID CONFIG - DEFAULTS ACTIVE");
                    app_logs_add(APP_LOG_LEVEL_ERROR, "SETTINGS",
                                 "CONFIG CRC OR VERSION ERROR");
                }
                else
                {
                    app_settings_set_status("CONFIG LOAD FAILED - DEFAULTS ACTIVE");
                    app_logs_add(APP_LOG_LEVEL_ERROR, "SETTINGS",
                                 "CONFIG LOAD FAILED");
                }
            }
        }
        else if (response.operation == APP_STORAGE_OP_WRITE_SETTINGS)
        {
            if (response.result == APP_STORAGE_RESULT_OK)
            {
                g_settings.dirty = 0U;
                app_settings_set_status("SETTINGS.CFG SAVED TO SD CARD");
                app_logs_add(APP_LOG_LEVEL_INFO, "SETTINGS", "CONFIG SAVED");
            }
            else
            {
                g_settings.save_pending = 1U;
                g_settings.save_retry_tick = xTaskGetTickCount();
                app_settings_set_status("SETTINGS.CFG SAVE FAILED");
                app_logs_add(APP_LOG_LEVEL_ERROR, "SETTINGS", "CONFIG SAVE FAILED");
            }
        }

        /* A music-page volume change may arrive while a read or write owns
           io_document. Apply the newest value only after that transaction
           completes, then persist one consolidated follow-up document. */
        if (g_settings.pending_volume_valid)
        {
            defaults = app_settings_get_config();
            defaults.volume_percent = g_settings.pending_volume;
            g_settings.pending_volume_valid = 0U;
            app_settings_set_config(&defaults);
            g_settings.dirty = 1U;
            g_settings.save_pending = 1U;
        }
        app_settings_redraw();
    }

    if (!g_settings.busy && g_settings.dirty && g_settings.save_pending &&
        (TickType_t)(xTaskGetTickCount() - g_settings.save_retry_tick) >=
        pdMS_TO_TICKS(250U))
    {
        app_settings_save();
    }
}

/**
 * @brief app_settings_get_screen_timeout_seconds：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint32_t app_settings_get_screen_timeout_seconds(void)
{
    return app_settings_get_config().idle_timeout_seconds;
}

/**
 * @brief app_settings_get_serial_output_enabled：检查函数名所描述的条件是否成立，并返回明确的判断结果。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t app_settings_get_serial_output_enabled(void)
{
    return app_settings_get_config().serial_output_enabled;
}

/**
 * @brief app_settings_get_cursor_sensitivity：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t app_settings_get_cursor_sensitivity(void)
{
    return app_settings_get_config().cursor_sensitivity;
}

/**
 * @brief app_settings_get_cursor_size：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t app_settings_get_cursor_size(void)
{
    return app_settings_get_config().cursor_size;
}

/**
 * @brief app_settings_get_brightness_percent：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t app_settings_get_brightness_percent(void)
{
    return app_settings_get_config().brightness_percent;
}

/**
 * @brief app_settings_get_volume_percent：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t app_settings_get_volume_percent(void)
{
    return app_settings_get_config().volume_percent;
}

BaseType_t app_settings_set_volume_percent(uint8_t volume_percent)
{
    app_settings_config_t config;

    app_settings_init();
    if (!app_settings_volume_is_valid(volume_percent))
    {
        return pdFAIL;
    }

    config = app_settings_get_config();
    if (config.volume_percent == volume_percent)
    {
        return pdPASS;
    }
    config.volume_percent = volume_percent;
    app_settings_set_config(&config);
    g_settings.dirty = 1U;
    app_settings_set_status("VOLUME CHANGED IN MUSIC - AUTO SAVE");
    if (g_settings.busy)
    {
        g_settings.pending_volume = volume_percent;
        g_settings.pending_volume_valid = 1U;
        g_settings.save_pending = 1U;
    }
    else
    {
        app_settings_save();
    }
    return pdPASS;
}
