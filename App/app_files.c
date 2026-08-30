#include "app_files.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "app_logs.h"
#include "app_storage.h"
#include "app_ui.h"

typedef enum
{
    APP_FILES_VIEW_LIST = 0,
    APP_FILES_VIEW_CONTENT
} app_files_view_t;

typedef struct
{
    uint8_t active;
    uint8_t busy;
    uint8_t file_count;
    uint8_t page;
    int8_t selected_file;
    app_files_view_t view;
    app_storage_operation_t pending_operation;
    uint16_t next_file_number;
    uint16_t content_length;
    uint32_t open_file_size;
    uint8_t dirty;
    uint8_t delete_armed;
    uint8_t read_only;
    char open_name[APP_STORAGE_NAME_LENGTH];
    char focus_name[APP_STORAGE_NAME_LENGTH];
    char content[APP_STORAGE_CONTENT_SIZE];
    char status[64];
    char after_list_status[64];
    app_storage_file_t files[APP_STORAGE_MAX_FILES];
} app_files_state_t;

static app_files_state_t g_files;

static void app_files_copy_text(char *destination,
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

static uint8_t app_files_is_text_file(const char *name)
{
    const char *extension;

    extension = strrchr(name, '.');
    if (extension == NULL)
    {
        return 0U;
    }
    return (strcmp(extension, ".TXT") == 0 ||
            strcmp(extension, ".txt") == 0) ? 1U : 0U;
}

static void app_files_redraw(void)
{
    if (!g_files.active)
    {
        return;
    }

    if (g_files.view == APP_FILES_VIEW_CONTENT)
    {
        app_ui_show_file_content(g_files.open_name,
                                 g_files.content,
                                 g_files.open_file_size,
                                 g_files.dirty,
                                 g_files.status,
                                 g_files.busy);
    }
    else
    {
        app_ui_show_files_list(g_files.files,
                               g_files.file_count,
                               g_files.page,
                               g_files.selected_file,
                               g_files.status,
                               g_files.busy);
    }
}

static void app_files_set_status(const char *status)
{
    app_files_copy_text(g_files.status, sizeof(g_files.status), status);
}

static BaseType_t app_files_submit(app_storage_request_t *request,
                                   const char *busy_status)
{
    if (g_files.busy)
    {
        app_files_set_status("ANOTHER FILE OPERATION IS ACTIVE");
        app_files_redraw();
        return pdFAIL;
    }

    if (app_storage_submit(request) != pdPASS)
    {
        app_files_set_status("STORAGE REQUEST QUEUE IS FULL");
        app_files_redraw();
        return pdFAIL;
    }

    g_files.busy = 1U;
    g_files.pending_operation = request->operation;
    app_files_set_status(busy_status);
    app_files_redraw();
    return pdPASS;
}

static void app_files_request_list(const char *after_status)
{
    app_storage_request_t request;

    memset(&request, 0, sizeof(request));
    request.operation = APP_STORAGE_OP_LIST;
    app_files_copy_text(g_files.after_list_status,
                        sizeof(g_files.after_list_status), after_status);
    app_files_submit(&request, "READING SD DIRECTORY");
}

static uint8_t app_files_name_exists(const char *name)
{
    uint8_t index;

    for (index = 0U; index < g_files.file_count; index++)
    {
        if (strcmp(g_files.files[index].name, name) == 0)
        {
            return 1U;
        }
    }
    return 0U;
}

static void app_files_make_new_name(char name[APP_STORAGE_NAME_LENGTH])
{
    uint16_t number;

    number = g_files.next_file_number;
    while (number < 10000U)
    {
        name[0] = 'N';
        name[1] = 'O';
        name[2] = 'T';
        name[3] = 'E';
        name[4] = (char)('0' + (number / 1000U) % 10U);
        name[5] = (char)('0' + (number / 100U) % 10U);
        name[6] = (char)('0' + (number / 10U) % 10U);
        name[7] = (char)('0' + number % 10U);
        name[8] = '.';
        name[9] = 'T';
        name[10] = 'X';
        name[11] = 'T';
        name[12] = '\0';
        number++;
        if (!app_files_name_exists(name))
        {
            g_files.next_file_number = number;
            return;
        }
    }

    app_files_copy_text(name, APP_STORAGE_NAME_LENGTH, "NEWFILE.TXT");
}

static void app_files_create(void)
{
    static const char initial_content[] =
        "Created by STM32 FreeRTOS\r\nTouch EDIT then SAVE.\r\n";
    app_storage_request_t request;

    memset(&request, 0, sizeof(request));
    request.operation = APP_STORAGE_OP_CREATE;
    app_files_make_new_name(request.name);
    app_files_copy_text(request.content, sizeof(request.content), initial_content);
    request.content_length = (uint16_t)strlen(request.content);
    app_files_copy_text(g_files.focus_name, sizeof(g_files.focus_name), request.name);
    g_files.delete_armed = 0U;
    app_files_submit(&request, "CREATING FILE ON SD CARD");
}

static void app_files_read_selected(void)
{
    app_storage_request_t request;

    if (g_files.selected_file < 0 ||
        (uint8_t)g_files.selected_file >= g_files.file_count)
    {
        app_files_set_status("SELECT A FILE FIRST");
        app_files_redraw();
        return;
    }

    memset(&request, 0, sizeof(request));
    request.operation = APP_STORAGE_OP_READ;
    app_files_copy_text(request.name, sizeof(request.name),
                        g_files.files[(uint8_t)g_files.selected_file].name);
    g_files.delete_armed = 0U;
    app_files_submit(&request, "READING FILE FROM SD CARD");
}

static void app_files_save(void)
{
    app_storage_request_t request;

    if (g_files.read_only)
    {
        app_files_set_status("THIS FILE IS READ ONLY IN THE DEMO");
        app_files_redraw();
        return;
    }
    if (!g_files.dirty)
    {
        app_files_set_status("NO CHANGES TO SAVE");
        app_files_redraw();
        return;
    }

    memset(&request, 0, sizeof(request));
    request.operation = APP_STORAGE_OP_WRITE;
    app_files_copy_text(request.name, sizeof(request.name), g_files.open_name);
    memcpy(request.content, g_files.content, g_files.content_length);
    request.content_length = g_files.content_length;
    g_files.delete_armed = 0U;
    app_files_submit(&request, "SAVING FILE TO SD CARD");
}

static void app_files_edit(void)
{
    static const char edit_line[] = "\r\nEDITED BY STM32\r\n";
    uint16_t edit_length;

    if (g_files.read_only)
    {
        app_files_set_status("ONLY SMALL TEXT FILES CAN BE EDITED");
        app_files_redraw();
        return;
    }

    edit_length = (uint16_t)(sizeof(edit_line) - 1U);
    if ((uint32_t)g_files.content_length + edit_length >=
        APP_STORAGE_CONTENT_SIZE)
    {
        app_files_set_status("EDITOR BUFFER IS FULL");
        app_files_redraw();
        return;
    }

    memcpy(&g_files.content[g_files.content_length], edit_line, edit_length);
    g_files.content_length = (uint16_t)(g_files.content_length + edit_length);
    g_files.content[g_files.content_length] = '\0';
    g_files.open_file_size = g_files.content_length;
    g_files.dirty = 1U;
    g_files.delete_armed = 0U;
    app_files_set_status("MODIFIED IN RAM - TOUCH SAVE");
    app_files_redraw();
}

static void app_files_delete_name(const char *name)
{
    app_storage_request_t request;

    if (!g_files.delete_armed)
    {
        g_files.delete_armed = 1U;
        app_files_set_status("TOUCH DELETE AGAIN TO CONFIRM");
        app_files_redraw();
        return;
    }

    memset(&request, 0, sizeof(request));
    request.operation = APP_STORAGE_OP_DELETE;
    app_files_copy_text(request.name, sizeof(request.name), name);
    app_files_copy_text(g_files.focus_name, sizeof(g_files.focus_name), "");
    g_files.delete_armed = 0U;
    app_files_submit(&request, "DELETING FILE FROM SD CARD");
}

static const char *app_files_error_text(app_storage_result_t result)
{
    switch (result)
    {
        case APP_STORAGE_RESULT_NOT_READY:
            return "SD CARD OR FAT32 IS NOT READY";

        case APP_STORAGE_RESULT_EXISTS:
            return "FILE NAME ALREADY EXISTS";

        case APP_STORAGE_RESULT_NOT_FOUND:
            return "FILE WAS NOT FOUND";

        case APP_STORAGE_RESULT_INVALID_NAME:
            return "INVALID FAT32 FILE NAME";

        case APP_STORAGE_RESULT_QUEUE_FULL:
            return "STORAGE QUEUE IS FULL";

        case APP_STORAGE_RESULT_IO_ERROR:
        default:
            return "SD CARD FILE OPERATION FAILED";
    }
}

static void app_files_select_focus(void)
{
    uint8_t index;

    if (g_files.focus_name[0] == '\0')
    {
        return;
    }
    for (index = 0U; index < g_files.file_count; index++)
    {
        if (strcmp(g_files.files[index].name, g_files.focus_name) == 0)
        {
            g_files.selected_file = (int8_t)index;
            g_files.page = (uint8_t)(index / APP_UI_FILES_PAGE_SIZE);
            break;
        }
    }
    g_files.focus_name[0] = '\0';
}

static void app_files_handle_response(const app_storage_response_t *response)
{
    g_files.busy = 0U;
    if (!g_files.active)
    {
        return;
    }
    if (response->result != APP_STORAGE_RESULT_OK)
    {
        app_logs_add(APP_LOG_LEVEL_ERROR, "FILES", "FILE OPERATION FAILED");
        app_files_set_status(app_files_error_text(response->result));
        app_files_redraw();
        return;
    }

    switch (response->operation)
    {
        case APP_STORAGE_OP_LIST:
            g_files.file_count = response->file_count;
            memcpy(g_files.files, response->files, sizeof(g_files.files));
            if (g_files.file_count == 0U)
            {
                g_files.selected_file = -1;
                g_files.page = 0U;
            }
            else if (g_files.selected_file >= (int8_t)g_files.file_count)
            {
                g_files.selected_file = (int8_t)(g_files.file_count - 1U);
                g_files.page = (uint8_t)((uint8_t)g_files.selected_file /
                                         APP_UI_FILES_PAGE_SIZE);
            }
            app_files_select_focus();
            if (g_files.after_list_status[0] != '\0')
            {
                app_files_set_status(g_files.after_list_status);
                g_files.after_list_status[0] = '\0';
            }
            else
            {
                app_files_set_status("SD DIRECTORY LOADED");
            }
            app_files_redraw();
            app_logs_add(APP_LOG_LEVEL_INFO, "FILES", "DIRECTORY LOADED");
            break;

        case APP_STORAGE_OP_CREATE:
            app_logs_add(APP_LOG_LEVEL_INFO, "FILES", "FILE CREATED");
            app_files_request_list("FILE CREATED AND SAVED");
            break;

        case APP_STORAGE_OP_READ:
            app_files_copy_text(g_files.open_name, sizeof(g_files.open_name),
                                response->name);
            memcpy(g_files.content, response->content,
                   APP_STORAGE_CONTENT_SIZE);
            g_files.content[APP_STORAGE_CONTENT_SIZE - 1U] = '\0';
            g_files.content_length = response->content_length;
            g_files.open_file_size = response->file_size;
            g_files.dirty = 0U;
            g_files.delete_armed = 0U;
            g_files.read_only = (!app_files_is_text_file(response->name) ||
                                 response->file_size >= APP_STORAGE_CONTENT_SIZE) ? 1U : 0U;
            g_files.view = APP_FILES_VIEW_CONTENT;
            app_files_set_status(g_files.read_only ?
                                 "VIEW ONLY - FILE IS BINARY OR TOO LARGE" :
                                 "FILE READ FROM SD CARD");
            app_logs_add(APP_LOG_LEVEL_INFO, "FILES", "FILE OPENED");
            app_files_redraw();
            break;

        case APP_STORAGE_OP_WRITE:
            g_files.dirty = 0U;
            g_files.open_file_size = response->content_length;
            app_files_set_status("FILE SAVED - DATA IS PERSISTENT");
            app_logs_add(APP_LOG_LEVEL_INFO, "FILES", "FILE SAVED");
            app_files_redraw();
            break;

        case APP_STORAGE_OP_DELETE:
            g_files.view = APP_FILES_VIEW_LIST;
            g_files.selected_file = -1;
            g_files.open_name[0] = '\0';
            g_files.content[0] = '\0';
            app_logs_add(APP_LOG_LEVEL_WARNING, "FILES", "FILE DELETED");
            app_files_request_list("FILE DELETED FROM SD CARD");
            break;

        default:
            app_files_set_status("UNEXPECTED STORAGE RESPONSE");
            app_files_redraw();
            break;
    }
}

void app_files_open(void)
{
    memset(&g_files, 0, sizeof(g_files));
    g_files.active = 1U;
    g_files.selected_file = -1;
    g_files.next_file_number = 1U;
    g_files.view = APP_FILES_VIEW_LIST;
    app_files_set_status("OPENING SD CARD DIRECTORY");
    app_files_redraw();
    app_files_request_list(NULL);
}

void app_files_close(void)
{
    g_files.active = 0U;
}

void app_files_handle_event(const app_input_event_t *event)
{
    app_ui_files_action_t action;
    int8_t row;
    uint8_t index;
    uint8_t page_count;

    if (!g_files.active || event == NULL || g_files.busy)
    {
        return;
    }

    if (event->type == APP_INPUT_EVENT_SCROLL)
    {
        if (g_files.view == APP_FILES_VIEW_CONTENT)
        {
            return;
        }
        page_count = (uint8_t)((g_files.file_count +
                                APP_UI_FILES_PAGE_SIZE - 1U) /
                               APP_UI_FILES_PAGE_SIZE);
        if (event->wheel > 0 && g_files.page > 0U)
        {
            g_files.page--;
            g_files.delete_armed = 0U;
            app_files_set_status("PREVIOUS PAGE");
            app_files_redraw();
        }
        else if (event->wheel < 0 && g_files.page + 1U < page_count)
        {
            g_files.page++;
            g_files.delete_armed = 0U;
            app_files_set_status("NEXT PAGE");
            app_files_redraw();
        }
        return;
    }

    if (event->type != APP_INPUT_EVENT_DOWN)
    {
        return;
    }

    action = app_ui_files_action_at(event->x, event->y,
                                    g_files.view == APP_FILES_VIEW_CONTENT);
    if (g_files.view == APP_FILES_VIEW_CONTENT)
    {
        if (action == APP_UI_FILES_ACTION_EDIT)
        {
            app_files_edit();
        }
        else if (action == APP_UI_FILES_ACTION_SAVE)
        {
            app_files_save();
        }
        else if (action == APP_UI_FILES_ACTION_DELETE)
        {
            app_files_delete_name(g_files.open_name);
        }
        else if (action == APP_UI_FILES_ACTION_LIST)
        {
            g_files.view = APP_FILES_VIEW_LIST;
            g_files.delete_armed = 0U;
            app_files_set_status(g_files.dirty ?
                                 "UNSAVED CHANGES WERE NOT WRITTEN" :
                                 "FILE LIST");
            app_files_redraw();
        }
        return;
    }

    row = app_ui_files_row_at(event->x, event->y);
    if (row >= 0)
    {
        index = (uint8_t)(g_files.page * APP_UI_FILES_PAGE_SIZE + (uint8_t)row);
        if (index < g_files.file_count)
        {
            g_files.selected_file = (int8_t)index;
            g_files.delete_armed = 0U;
            app_files_set_status("FILE SELECTED");
            app_files_redraw();
        }
        return;
    }

    if (action == APP_UI_FILES_ACTION_NEW)
    {
        app_files_create();
    }
    else if (action == APP_UI_FILES_ACTION_OPEN)
    {
        app_files_read_selected();
    }
    else if (action == APP_UI_FILES_ACTION_DELETE)
    {
        if (g_files.selected_file >= 0 &&
            (uint8_t)g_files.selected_file < g_files.file_count)
        {
            app_files_delete_name(
                g_files.files[(uint8_t)g_files.selected_file].name);
        }
    }
    else if (action == APP_UI_FILES_ACTION_PREVIOUS && g_files.page > 0U)
    {
        g_files.page--;
        g_files.delete_armed = 0U;
        app_files_set_status("PREVIOUS PAGE");
        app_files_redraw();
    }
    else if (action == APP_UI_FILES_ACTION_NEXT)
    {
        page_count = (uint8_t)((g_files.file_count + APP_UI_FILES_PAGE_SIZE - 1U) /
                               APP_UI_FILES_PAGE_SIZE);
        if (g_files.page + 1U < page_count)
        {
            g_files.page++;
            g_files.delete_armed = 0U;
            app_files_set_status("NEXT PAGE");
            app_files_redraw();
        }
    }
    else if (action == APP_UI_FILES_ACTION_REFRESH)
    {
        g_files.delete_armed = 0U;
        app_files_request_list(NULL);
    }
}

void app_files_update(void)
{
    static app_storage_response_t response;

    while (app_storage_receive(&response) == pdPASS)
    {
        app_files_handle_response(&response);
    }
}
