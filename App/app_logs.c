/**
 * @file app_logs.c
 * @brief 维护运行日志缓冲区并向日志界面提供筛选和显示数据。
 * @details 这是 app_logs 模块的实现文件（App/app_logs.c）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#include "app_logs.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "app_storage.h"
#include "app_ui.h"
#include "task.h"

/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define APP_LOG_EXPORT_FILE_NAME  "SYSTEM.LOG"
#define APP_LOG_EXPORT_SIZE       6144U

/** @brief 模块数据类型：描述本模块维护的状态、配置或数据快照。 */
typedef struct
{
    uint8_t initialized;
    uint8_t count;
    uint8_t next;
    uint32_t generation;
    app_log_entry_t entries[APP_LOG_MAX_ENTRIES];
} app_log_repository_t;

typedef struct
{
    uint8_t active;
    uint8_t busy;
    uint8_t clear_armed;
    uint8_t page;
    uint8_t snapshot_count;
    uint32_t snapshot_generation;
    char status[64];
    char export_buffer[APP_LOG_EXPORT_SIZE];
    app_log_entry_t snapshot[APP_LOG_MAX_ENTRIES];
} app_logs_view_t;

static app_log_repository_t g_log_repository;
static app_logs_view_t g_logs_view;

/**
 * @brief app_logs_copy_text：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param destination 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param destination_size 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param source 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void app_logs_copy_text(char *destination,
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
 * @brief app_logs_set_status：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param status 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void app_logs_set_status(const char *status)
{
    app_logs_copy_text(g_logs_view.status,
                       sizeof(g_logs_view.status), status);
}

/**
 * @brief app_logs_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_logs_init(void)
{
    taskENTER_CRITICAL();
    if (!g_log_repository.initialized)
    {
        memset(&g_log_repository, 0, sizeof(g_log_repository));
        g_log_repository.initialized = 1U;
        g_log_repository.generation = 1U;
    }
    taskEXIT_CRITICAL();
}

/**
 * @brief app_logs_add：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param level 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param module 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param message 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_logs_add(app_log_level_t level,
                  const char *module,
                  const char *message)
{
    app_log_entry_t *entry;

    app_logs_init();
    taskENTER_CRITICAL();
    entry = &g_log_repository.entries[g_log_repository.next];
    entry->uptime_seconds = (uint32_t)(xTaskGetTickCount() /
                                      configTICK_RATE_HZ);
    entry->level = level;
    app_logs_copy_text(entry->module, sizeof(entry->module), module);
    app_logs_copy_text(entry->message, sizeof(entry->message), message);

    g_log_repository.next = (uint8_t)((g_log_repository.next + 1U) %
                                      APP_LOG_MAX_ENTRIES);
    if (g_log_repository.count < APP_LOG_MAX_ENTRIES)
    {
        g_log_repository.count++;
    }
    g_log_repository.generation++;
    taskEXIT_CRITICAL();
}

/**
 * @brief app_logs_take_snapshot：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param reset_page 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void app_logs_take_snapshot(uint8_t reset_page)
{
    uint8_t index;
    uint8_t source_index;
    uint8_t page_count;

    taskENTER_CRITICAL();
    g_logs_view.snapshot_count = g_log_repository.count;
    for (index = 0U; index < g_log_repository.count; index++)
    {
        source_index = (uint8_t)((g_log_repository.next +
                                 APP_LOG_MAX_ENTRIES - 1U - index) %
                                APP_LOG_MAX_ENTRIES);
        g_logs_view.snapshot[index] = g_log_repository.entries[source_index];
    }
    g_logs_view.snapshot_generation = g_log_repository.generation;
    taskEXIT_CRITICAL();

    page_count = (uint8_t)((g_logs_view.snapshot_count +
                            APP_UI_LOGS_PAGE_SIZE - 1U) /
                           APP_UI_LOGS_PAGE_SIZE);
    if (reset_page || page_count == 0U)
    {
        g_logs_view.page = 0U;
    }
    else if (g_logs_view.page >= page_count)
    {
        g_logs_view.page = (uint8_t)(page_count - 1U);
    }
}

/**
 * @brief app_logs_redraw：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
static void app_logs_redraw(void)
{
    if (!g_logs_view.active)
    {
        return;
    }
    app_ui_show_logs(g_logs_view.snapshot,
                     g_logs_view.snapshot_count,
                     g_logs_view.page,
                     g_logs_view.status,
                     g_logs_view.busy,
                     g_logs_view.clear_armed);
}

/**
 * @brief app_logs_level_text：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param level 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static const char *app_logs_level_text(app_log_level_t level)
{
    if (level == APP_LOG_LEVEL_ERROR)
    {
        return "ERROR";
    }
    if (level == APP_LOG_LEVEL_WARNING)
    {
        return "WARN";
    }
    return "INFO";
}

/**
 * @brief app_logs_format_time：将输入值转换为调用方所需的数据格式或表示形式。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param seconds 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param text 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void app_logs_format_time(uint32_t seconds, char text[9])
{
    uint32_t hours;
    uint32_t minutes;

    seconds %= 86400U;
    hours = seconds / 3600U;
    minutes = (seconds % 3600U) / 60U;
    seconds %= 60U;
    text[0] = (char)('0' + hours / 10U);
    text[1] = (char)('0' + hours % 10U);
    text[2] = ':';
    text[3] = (char)('0' + minutes / 10U);
    text[4] = (char)('0' + minutes % 10U);
    text[5] = ':';
    text[6] = (char)('0' + seconds / 10U);
    text[7] = (char)('0' + seconds % 10U);
    text[8] = '\0';
}

/**
 * @brief app_logs_build_export：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint32_t app_logs_build_export(void)
{
    uint32_t used;
    int written;
    uint8_t index;
    char time_text[9];
    const app_log_entry_t *entry;

    used = 0U;
    written = snprintf(g_logs_view.export_buffer,
                       sizeof(g_logs_view.export_buffer),
                       "STM32H743 SYSTEM LOG\r\n"
                       "TIME     LEVEL MODULE   MESSAGE\r\n");
    if (written < 0 || (uint32_t)written >= sizeof(g_logs_view.export_buffer))
    {
        return 0U;
    }
    used = (uint32_t)written;

    for (index = g_logs_view.snapshot_count; index > 0U; index--)
    {
        entry = &g_logs_view.snapshot[index - 1U];
        app_logs_format_time(entry->uptime_seconds, time_text);
        written = snprintf(&g_logs_view.export_buffer[used],
                           sizeof(g_logs_view.export_buffer) - used,
                           "%s %-5s %-8s %s\r\n",
                           time_text,
                           app_logs_level_text(entry->level),
                           entry->module,
                           entry->message);
        if (written < 0 ||
            (uint32_t)written >= sizeof(g_logs_view.export_buffer) - used)
        {
            return 0U;
        }
        used += (uint32_t)written;
    }
    return used;
}

/**
 * @brief app_logs_export：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
static void app_logs_export(void)
{
    app_storage_request_t request;
    uint32_t export_length;

    app_logs_take_snapshot(0U);
    export_length = app_logs_build_export();
    if (export_length == 0U)
    {
        app_logs_set_status("LOG EXPORT BUFFER ERROR");
        app_logs_redraw();
        return;
    }

    memset(&request, 0, sizeof(request));
    request.operation = APP_STORAGE_OP_WRITE_LOG;
    app_logs_copy_text(request.name, sizeof(request.name),
                       APP_LOG_EXPORT_FILE_NAME);
    request.binary_data = g_logs_view.export_buffer;
    request.binary_length = export_length;
    if (app_storage_submit(&request) != pdPASS)
    {
        app_logs_set_status("STORAGE REQUEST QUEUE IS FULL");
        app_logs_redraw();
        return;
    }

    g_logs_view.busy = 1U;
    g_logs_view.clear_armed = 0U;
    app_logs_set_status("EXPORTING SYSTEM.LOG TO SD CARD");
    app_logs_redraw();
}

/**
 * @brief app_logs_clear：清除已有状态或复位目标设备，使其回到约定的初始状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
static void app_logs_clear(void)
{
    taskENTER_CRITICAL();
    g_log_repository.count = 0U;
    g_log_repository.next = 0U;
    g_log_repository.generation++;
    taskEXIT_CRITICAL();

    g_logs_view.clear_armed = 0U;
    app_logs_set_status("RAM LOG BUFFER CLEARED");
    app_logs_take_snapshot(1U);
    app_logs_redraw();
}

/**
 * @brief app_logs_open：启动或启用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_logs_open(void)
{
    app_logs_init();
    memset(&g_logs_view, 0, sizeof(g_logs_view));
    g_logs_view.active = 1U;
    app_logs_set_status("NEWEST EVENTS ARE SHOWN FIRST");
    app_logs_take_snapshot(1U);
    app_logs_redraw();
}

/**
 * @brief app_logs_close：停止或禁用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_logs_close(void)
{
    g_logs_view.active = 0U;
    g_logs_view.clear_armed = 0U;
}

/**
 * @brief app_logs_handle_event：解析并处理当前事件或数据，根据结果推进模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param event 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_logs_handle_event(const app_input_event_t *event)
{
    app_ui_logs_action_t action;
    uint8_t page_count;

    if (!g_logs_view.active || event == NULL || g_logs_view.busy)
    {
        return;
    }

    page_count = (uint8_t)((g_logs_view.snapshot_count +
                            APP_UI_LOGS_PAGE_SIZE - 1U) /
                           APP_UI_LOGS_PAGE_SIZE);
    if (event->type == APP_INPUT_EVENT_SCROLL)
    {
        if (event->wheel > 0 && g_logs_view.page > 0U)
        {
            g_logs_view.page--;
            g_logs_view.clear_armed = 0U;
            app_logs_set_status("NEWER LOG PAGE");
            app_logs_redraw();
        }
        else if (event->wheel < 0 &&
                 g_logs_view.page + 1U < page_count)
        {
            g_logs_view.page++;
            g_logs_view.clear_armed = 0U;
            app_logs_set_status("OLDER LOG PAGE");
            app_logs_redraw();
        }
        return;
    }

    if (event->type != APP_INPUT_EVENT_DOWN)
    {
        return;
    }

    action = app_ui_logs_action_at(event->x, event->y);
    if (action == APP_UI_LOGS_ACTION_CLEAR)
    {
        if (!g_logs_view.clear_armed)
        {
            g_logs_view.clear_armed = 1U;
            app_logs_set_status("TOUCH CLEAR AGAIN TO CONFIRM");
            app_logs_redraw();
        }
        else
        {
            app_logs_clear();
        }
    }
    else if (action == APP_UI_LOGS_ACTION_EXPORT)
    {
        app_logs_export();
    }
    else if (action == APP_UI_LOGS_ACTION_PREVIOUS && g_logs_view.page > 0U)
    {
        g_logs_view.page--;
        g_logs_view.clear_armed = 0U;
        app_logs_set_status("NEWER LOG PAGE");
        app_logs_redraw();
    }
    else if (action == APP_UI_LOGS_ACTION_NEXT &&
             g_logs_view.page + 1U < page_count)
    {
        g_logs_view.page++;
        g_logs_view.clear_armed = 0U;
        app_logs_set_status("OLDER LOG PAGE");
        app_logs_redraw();
    }
}

/**
 * @brief app_logs_update：使用最新数据更新缓存、硬件输出或界面显示状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_logs_update(void)
{
    app_storage_binary_response_t response;
    uint32_t repository_generation;

    while (app_storage_receive_log(&response) == pdPASS)
    {
        g_logs_view.busy = 0U;
        if (response.result == APP_STORAGE_RESULT_OK)
        {
            app_logs_set_status("SYSTEM.LOG EXPORTED TO SD CARD");
            app_logs_add(APP_LOG_LEVEL_INFO, "LOGS", "SYSTEM.LOG EXPORTED");
        }
        else
        {
            app_logs_set_status("SYSTEM.LOG EXPORT FAILED");
            app_logs_add(APP_LOG_LEVEL_ERROR, "LOGS", "SD EXPORT FAILED");
        }
    }

    taskENTER_CRITICAL();
    repository_generation = g_log_repository.generation;
    taskEXIT_CRITICAL();
    if (g_logs_view.active &&
        g_logs_view.snapshot_generation != repository_generation)
    {
        app_logs_take_snapshot(0U);
        app_logs_redraw();
    }
}
