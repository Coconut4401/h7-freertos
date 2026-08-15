#include "app_logs.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "app_storage.h"
#include "app_ui.h"
#include "task.h"

#define APP_LOG_EXPORT_FILE_NAME  "SYSTEM.LOG"
#define APP_LOG_EXPORT_SIZE       6144U

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

static void app_logs_set_status(const char *status)
{
    app_logs_copy_text(g_logs_view.status,
                       sizeof(g_logs_view.status), status);
}

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

void app_logs_open(void)
{
    app_logs_init();
    memset(&g_logs_view, 0, sizeof(g_logs_view));
    g_logs_view.active = 1U;
    app_logs_set_status("NEWEST EVENTS ARE SHOWN FIRST");
    app_logs_take_snapshot(1U);
    app_logs_redraw();
}

void app_logs_close(void)
{
    g_logs_view.active = 0U;
    g_logs_view.clear_armed = 0U;
}

void app_logs_handle_event(const app_input_event_t *event)
{
    app_ui_logs_action_t action;
    uint8_t page_count;

    if (!g_logs_view.active || event == NULL ||
        event->type != APP_INPUT_EVENT_DOWN || g_logs_view.busy)
    {
        return;
    }

    action = app_ui_logs_action_at(event->x, event->y);
    page_count = (uint8_t)((g_logs_view.snapshot_count +
                            APP_UI_LOGS_PAGE_SIZE - 1U) /
                           APP_UI_LOGS_PAGE_SIZE);
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
