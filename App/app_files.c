/**
 * @file app_files.c
 * @brief 实现文件浏览界面以及目录、文件信息的读取和展示。
 * @details 这是 app_files 模块的实现文件（App/app_files.c）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#include "app_files.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "app_logs.h"
#include "app_storage.h"
#include "app_ui.h"

/** @brief 模块数据类型：描述本模块维护的状态、配置或数据快照。 */
typedef enum
{
    APP_FILES_VIEW_LIST = 0,
    APP_FILES_VIEW_CONTENT
} app_files_view_t;

typedef enum
{
    APP_FILES_EDITOR_NONE = 0,
    APP_FILES_EDITOR_NEW_NAME,
    APP_FILES_EDITOR_CONTENT,
    APP_FILES_EDITOR_RENAME,
    APP_FILES_EDITOR_CONFIRM
} app_files_editor_t;

typedef struct
{
    uint8_t active;
    uint8_t busy;
    uint8_t file_count;
    uint8_t page;
    int8_t selected_file;
    app_files_view_t view;
    app_storage_operation_t pending_operation;
    uint32_t pending_request_id;
    uint16_t next_file_number;
    uint16_t content_length;
    uint32_t open_file_size;
    uint32_t content_offset;
    uint8_t dirty;
    uint8_t delete_armed;
    uint8_t read_only;
    uint8_t open_after_list;
    uint8_t edit_after_read;
    uint8_t leave_after_save;
    uint8_t exit_after_save;
    uint8_t close_requested;
    uint8_t confirm_exit;
    app_files_editor_t editor;
    app_ui_keyboard_mode_t keyboard_mode;
    uint16_t editor_length;
    char editor_name[APP_STORAGE_NAME_LENGTH];
    char editor_buffer[APP_STORAGE_CONTENT_SIZE];
    char open_name[APP_STORAGE_NAME_LENGTH];
    char focus_name[APP_STORAGE_NAME_LENGTH];
    char content[APP_STORAGE_CONTENT_SIZE];
    char status[64];
    char after_list_status[64];
    app_storage_file_t files[APP_STORAGE_MAX_FILES];
} app_files_state_t;

static app_files_state_t g_files;
/* Survives page reset so every FILES request has a distinct identity. */
static uint32_t g_files_next_request_id;

static void app_files_editor_redraw(void);
static void app_files_editor_cancel(void);
static void app_files_editor_commit(void);
static uint8_t app_files_editor_name_valid(const char *name);
static void app_files_editor_handle_key(const app_ui_keyboard_hit_t *hit);
static void app_files_start_content_editor(void);
static void app_files_start_name_editor(app_files_editor_t editor);
static void app_files_set_status(const char *status);
static uint8_t app_files_name_exists(const char *name);
static void app_files_make_new_name(char name[APP_STORAGE_NAME_LENGTH]);
static void app_files_redraw(void);
static BaseType_t app_files_submit(app_storage_request_t *request,
                                   const char *busy_status);
static void app_files_request_content(uint32_t offset);
static void app_files_set_content_status(void);

/**
 * @brief app_files_copy_text：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param destination 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param destination_size 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param source 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
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

/**
 * @brief app_files_is_text_file：检查函数名所描述的条件是否成立，并返回明确的判断结果。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param name 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint8_t app_files_is_text_file(const char *name)
{
    const char *extension;
    char first;
    char second;
    char third;

    extension = strrchr(name, '.');
    if (extension == NULL || strlen(extension) != 4U)
    {
        return 0U;
    }
    first = extension[1];
    second = extension[2];
    third = extension[3];
    if (first >= 'a' && first <= 'z') first = (char)(first - ('a' - 'A'));
    if (second >= 'a' && second <= 'z') second = (char)(second - ('a' - 'A'));
    if (third >= 'a' && third <= 'z') third = (char)(third - ('a' - 'A'));
    return (first == 'T' && second == 'X' && third == 'T') ? 1U : 0U;
}

static uint8_t app_files_editor_name_valid(const char *name)
{
    uint16_t length;
    uint8_t dot_seen;
    uint8_t base_length;
    uint8_t extension_length;
    uint8_t index;

    if (name == NULL || name[0] == '\0' || name[0] == '~')
    {
        return 0U;
    }
    length = (uint16_t)strlen(name);
    if (length == 0U || length >= APP_STORAGE_NAME_LENGTH)
    {
        return 0U;
    }
    dot_seen = 0U;
    base_length = 0U;
    extension_length = 0U;
    for (index = 0U; index < length; index++)
    {
        if (name[index] == '.')
        {
            if (dot_seen || base_length == 0U)
            {
                return 0U;
            }
            dot_seen = 1U;
            continue;
        }
        if (name[index] <= ' ' || name[index] == '/' || name[index] == '\\' ||
            name[index] == ':' || name[index] == '*' || name[index] == '?' ||
            name[index] == '"' || name[index] == '<' || name[index] == '>' ||
            name[index] == '|')
        {
            return 0U;
        }
        if (dot_seen) extension_length++;
        else base_length++;
    }
    if (dot_seen && extension_length == 0U)
    {
        return 0U;
    }
    return (base_length <= 8U && extension_length <= 3U) ? 1U : 0U;
}

static uint8_t app_files_editor_finalize_name(void)
{
    const char *extension;
    uint16_t length;
    uint16_t extension_length;

    length = (uint16_t)strlen(g_files.editor_buffer);
    if (strchr(g_files.editor_buffer, '.') == NULL)
    {
        extension = (g_files.editor == APP_FILES_EDITOR_RENAME) ?
                    strrchr(g_files.open_name, '.') : ".TXT";
        if (extension == NULL)
        {
            extension = "";
        }
        extension_length = (uint16_t)strlen(extension);
        if (length == 0U || length > 8U ||
            length + extension_length >= APP_STORAGE_NAME_LENGTH)
        {
            return 0U;
        }
        memcpy(&g_files.editor_buffer[length], extension,
               extension_length + 1U);
        g_files.editor_length = (uint16_t)(length + extension_length);
    }
    if (!app_files_editor_name_valid(g_files.editor_buffer))
    {
        return 0U;
    }
    return (g_files.editor == APP_FILES_EDITOR_NEW_NAME) ?
           app_files_is_text_file(g_files.editor_buffer) : 1U;
}

static void app_files_editor_redraw(void)
{
    const char *title;

    if (g_files.editor == APP_FILES_EDITOR_CONFIRM)
    {
        app_ui_show_file_confirmation(g_files.open_name,
                                      "SAVE CHANGES BEFORE LEAVING?");
        return;
    }
    title = (g_files.editor == APP_FILES_EDITOR_NEW_NAME) ?
            "NEW FILE NAME" :
            ((g_files.editor == APP_FILES_EDITOR_RENAME) ?
             "RENAME FILE" : "EDIT FILE CONTENT");
    app_ui_show_file_editor(title,
                            (g_files.editor == APP_FILES_EDITOR_CONTENT) ?
                                g_files.editor_name : g_files.editor_buffer,
                            (g_files.editor == APP_FILES_EDITOR_CONTENT) ?
                                g_files.editor_buffer : "",
                            g_files.editor_length,
                            (g_files.editor != APP_FILES_EDITOR_CONTENT),
                            g_files.status,
                            g_files.keyboard_mode);
}

static void app_files_editor_cancel(void)
{
    g_files.editor = APP_FILES_EDITOR_NONE;
    g_files.editor_length = 0U;
    g_files.editor_buffer[0] = '\0';
    g_files.editor_name[0] = '\0';
    app_files_set_status("EDITOR CANCELED");
    app_files_redraw();
}

static void app_files_editor_commit(void)
{
    if (g_files.editor == APP_FILES_EDITOR_NEW_NAME)
    {
        app_storage_request_t request;
        if (!app_files_editor_finalize_name() ||
            app_files_name_exists(g_files.editor_buffer))
        {
            app_files_set_status("INVALID OR DUPLICATE FILE NAME");
            app_files_editor_redraw();
            return;
        }
        memset(&request, 0, sizeof(request));
        request.operation = APP_STORAGE_OP_CREATE;
        app_files_copy_text(request.name, sizeof(request.name),
                            g_files.editor_buffer);
        request.content[0] = '\0';
        request.content_length = 0U;
        app_files_copy_text(g_files.focus_name, sizeof(g_files.focus_name),
                            request.name);
        g_files.editor = APP_FILES_EDITOR_NONE;
        if (app_files_submit(&request, "CREATING NEW FILE") != pdPASS)
        {
            g_files.editor = APP_FILES_EDITOR_NEW_NAME;
            app_files_editor_redraw();
        }
    }
    else if (g_files.editor == APP_FILES_EDITOR_RENAME)
    {
        app_storage_request_t request;
        if (!app_files_editor_finalize_name() ||
            (strcmp(g_files.editor_buffer, g_files.open_name) != 0 &&
             app_files_name_exists(g_files.editor_buffer)))
        {
            app_files_set_status("INVALID OR DUPLICATE FILE NAME");
            app_files_editor_redraw();
            return;
        }
        if (strcmp(g_files.editor_buffer, g_files.open_name) == 0)
        {
            app_files_editor_cancel();
            return;
        }
        memset(&request, 0, sizeof(request));
        request.operation = APP_STORAGE_OP_RENAME;
        app_files_copy_text(request.name, sizeof(request.name), g_files.open_name);
        app_files_copy_text(request.new_name, sizeof(request.new_name),
                            g_files.editor_buffer);
        app_files_submit(&request, "RENAMING FILE");
    }
    else if (g_files.editor == APP_FILES_EDITOR_CONTENT)
    {
        if (g_files.editor_length >= APP_STORAGE_CONTENT_SIZE)
        {
            app_files_set_status("EDITOR BUFFER IS FULL");
            app_files_editor_redraw();
            return;
        }
        memcpy(g_files.content, g_files.editor_buffer, g_files.editor_length);
        g_files.content[g_files.editor_length] = '\0';
        g_files.content_length = g_files.editor_length;
        g_files.open_file_size = g_files.editor_length;
        g_files.dirty = 1U;
        g_files.editor = APP_FILES_EDITOR_NONE;
        g_files.editor_length = 0U;
        g_files.editor_buffer[0] = '\0';
        g_files.editor_name[0] = '\0';
        app_files_set_status("CONTENT UPDATED - TOUCH SAVE");
        app_files_redraw();
    }
}

static void app_files_editor_handle_key(const app_ui_keyboard_hit_t *hit)
{
    char character;
    uint8_t redraw_keyboard;

    if (hit == NULL) return;
    redraw_keyboard = 0U;
    if (hit->action == APP_UI_KEYBOARD_ACTION_SHIFT)
    {
        g_files.keyboard_mode = (g_files.keyboard_mode == APP_UI_KEYBOARD_MODE_UPPER) ?
                                 APP_UI_KEYBOARD_MODE_LOWER : APP_UI_KEYBOARD_MODE_UPPER;
        redraw_keyboard = 1U;
    }
    else if (hit->action == APP_UI_KEYBOARD_ACTION_NUMBER)
    {
        g_files.keyboard_mode = (g_files.keyboard_mode == APP_UI_KEYBOARD_MODE_NUMBER) ?
                                 APP_UI_KEYBOARD_MODE_LOWER : APP_UI_KEYBOARD_MODE_NUMBER;
        redraw_keyboard = 1U;
    }
    else if (hit->action == APP_UI_KEYBOARD_ACTION_BACKSPACE)
    {
        if (g_files.editor_length > 0U)
        {
            g_files.editor_length--;
            if (g_files.editor == APP_FILES_EDITOR_CONTENT &&
                g_files.editor_length > 0U &&
                g_files.editor_buffer[g_files.editor_length] == '\n' &&
                g_files.editor_buffer[g_files.editor_length - 1U] == '\r')
            {
                g_files.editor_length--;
            }
            g_files.editor_buffer[g_files.editor_length] = '\0';
        }
    }
    else if (hit->action == APP_UI_KEYBOARD_ACTION_SPACE)
    {
        if (g_files.editor == APP_FILES_EDITOR_CONTENT &&
            g_files.editor_length + 1U < APP_STORAGE_CONTENT_SIZE)
        {
            g_files.editor_buffer[g_files.editor_length++] = ' ';
            g_files.editor_buffer[g_files.editor_length] = '\0';
        }
    }
    else if (hit->action == APP_UI_KEYBOARD_ACTION_ENTER)
    {
        if (g_files.editor == APP_FILES_EDITOR_CONTENT &&
            g_files.editor_length + 2U < APP_STORAGE_CONTENT_SIZE)
        {
            g_files.editor_buffer[g_files.editor_length++] = '\r';
            g_files.editor_buffer[g_files.editor_length++] = '\n';
            g_files.editor_buffer[g_files.editor_length] = '\0';
        }
    }
    else if (hit->action == APP_UI_KEYBOARD_ACTION_CHAR)
    {
        character = hit->character;
        if (g_files.editor == APP_FILES_EDITOR_CONTENT &&
            g_files.editor_length + 1U < APP_STORAGE_CONTENT_SIZE)
        {
            g_files.editor_buffer[g_files.editor_length++] = character;
            g_files.editor_buffer[g_files.editor_length] = '\0';
        }
        else if ((g_files.editor == APP_FILES_EDITOR_NEW_NAME ||
                  g_files.editor == APP_FILES_EDITOR_RENAME) &&
                 g_files.editor_length + 1U < APP_STORAGE_NAME_LENGTH &&
                 ((character >= 'A' && character <= 'Z') ||
                  (character >= 'a' && character <= 'z') ||
                  (character >= '0' && character <= '9') ||
                  character == '_' || character == '-' ||
                  (character == '.' &&
                   strchr(g_files.editor_buffer, '.') == NULL)))
        {
            g_files.editor_buffer[g_files.editor_length++] = character;
            g_files.editor_buffer[g_files.editor_length] = '\0';
        }
    }
    else if (hit->action == APP_UI_KEYBOARD_ACTION_OK)
    {
        app_files_editor_commit();
        return;
    }
    else if (hit->action == APP_UI_KEYBOARD_ACTION_CANCEL)
    {
        app_files_editor_cancel();
        return;
    }
    if (redraw_keyboard)
    {
        app_files_editor_redraw();
    }
    else
    {
        app_ui_update_file_editor(
            (g_files.editor == APP_FILES_EDITOR_NEW_NAME) ? "NEW FILE NAME" :
            ((g_files.editor == APP_FILES_EDITOR_RENAME) ?
             "RENAME FILE" : "EDIT FILE CONTENT"),
            (g_files.editor == APP_FILES_EDITOR_CONTENT) ?
                g_files.editor_name : g_files.editor_buffer,
            (g_files.editor == APP_FILES_EDITOR_CONTENT) ?
                g_files.editor_buffer : "",
            g_files.editor_length,
            (g_files.editor != APP_FILES_EDITOR_CONTENT),
            g_files.status,
            g_files.keyboard_mode);
    }
}

static void app_files_start_name_editor(app_files_editor_t editor)
{
    const char *extension;
    uint16_t base_length;

    g_files.editor = editor;
    g_files.keyboard_mode = APP_UI_KEYBOARD_MODE_UPPER;
    if (editor == APP_FILES_EDITOR_RENAME)
    {
        extension = strrchr(g_files.open_name, '.');
        base_length = (extension != NULL) ?
                      (uint16_t)(extension - g_files.open_name) :
                      (uint16_t)strlen(g_files.open_name);
        if (base_length >= APP_STORAGE_NAME_LENGTH)
        {
            base_length = APP_STORAGE_NAME_LENGTH - 1U;
        }
        memcpy(g_files.editor_buffer, g_files.open_name, base_length);
        g_files.editor_buffer[base_length] = '\0';
    }
    else
    {
        app_files_make_new_name(g_files.editor_buffer);
    }
    g_files.editor_length = (uint16_t)strlen(g_files.editor_buffer);
    app_files_set_status("TYPE NAME THEN PRESS OK");
    app_files_editor_redraw();
}

static void app_files_start_content_editor(void)
{
    if (g_files.read_only)
    {
        app_files_set_status("ONLY TEXT FILES SMALLER THAN 256B CAN BE EDITED");
        app_files_redraw();
        return;
    }
    g_files.editor = APP_FILES_EDITOR_CONTENT;
    g_files.keyboard_mode = APP_UI_KEYBOARD_MODE_LOWER;
    g_files.editor_length = g_files.content_length;
    if (g_files.editor_length >= APP_STORAGE_CONTENT_SIZE)
    {
        g_files.editor_length = APP_STORAGE_CONTENT_SIZE - 1U;
    }
    memcpy(g_files.editor_buffer, g_files.content, g_files.editor_length);
    g_files.editor_buffer[g_files.editor_length] = '\0';
    app_files_copy_text(g_files.editor_name, sizeof(g_files.editor_name),
                        g_files.open_name);
    app_files_set_status("EDITING COPY - PRESS OK TO APPLY");
    app_files_editor_redraw();
}

/**
 * @brief app_files_redraw：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
static void app_files_redraw(void)
{
    if (!g_files.active)
    {
        return;
    }

    if (g_files.editor != APP_FILES_EDITOR_NONE)
    {
        app_files_editor_redraw();
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

/**
 * @brief app_files_set_status：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param status 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void app_files_set_status(const char *status)
{
    app_files_copy_text(g_files.status, sizeof(g_files.status), status);
}

/**
 * @brief app_files_submit：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param request 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param busy_status 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static BaseType_t app_files_submit(app_storage_request_t *request,
                                   const char *busy_status)
{
    if (g_files.busy)
    {
        app_files_set_status("ANOTHER FILE OPERATION IS ACTIVE");
        app_files_redraw();
        return pdFAIL;
    }

    g_files_next_request_id++;
    if (g_files_next_request_id == 0U) g_files_next_request_id = 1U;
    request->request_id = g_files_next_request_id;
    if (app_storage_submit(request) != pdPASS)
    {
        app_files_set_status("STORAGE REQUEST QUEUE IS FULL");
        app_files_redraw();
        return pdFAIL;
    }

    g_files.busy = 1U;
    g_files.pending_request_id = request->request_id;
    g_files.pending_operation = request->operation;
    app_files_set_status(busy_status);
    app_files_redraw();
    return pdPASS;
}

/**
 * @brief app_files_request_list：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param after_status 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void app_files_request_list(const char *after_status)
{
    app_storage_request_t request;

    memset(&request, 0, sizeof(request));
    request.operation = APP_STORAGE_OP_LIST;
    app_files_copy_text(g_files.after_list_status,
                        sizeof(g_files.after_list_status), after_status);
    app_files_submit(&request, "READING SD DIRECTORY");
}

/**
 * @brief app_files_name_exists：将输入值转换为调用方所需的数据格式或表示形式。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param name 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint8_t app_files_name_exists(const char *name)
{
    uint8_t index;
    uint8_t character_index;
    char first;
    char second;

    for (index = 0U; index < g_files.file_count; index++)
    {
        character_index = 0U;
        do
        {
            first = g_files.files[index].name[character_index];
            second = name[character_index];
            if (first >= 'a' && first <= 'z') first = (char)(first - ('a' - 'A'));
            if (second >= 'a' && second <= 'z') second = (char)(second - ('a' - 'A'));
            if (first != second) break;
            if (first == '\0') return 1U;
            character_index++;
        } while (character_index < APP_STORAGE_NAME_LENGTH);
    }
    return 0U;
}

/**
 * @brief app_files_make_new_name：将输入值转换为调用方所需的数据格式或表示形式。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param name 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
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

/**
 * @brief app_files_create：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
static void app_files_create(void)
{
    app_files_start_name_editor(APP_FILES_EDITOR_NEW_NAME);
}

/**
 * @brief app_files_read_selected：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
static void app_files_read_selected(void)
{
    if (g_files.selected_file < 0 ||
        (uint8_t)g_files.selected_file >= g_files.file_count)
    {
        app_files_set_status("SELECT A FILE FIRST");
        app_files_redraw();
        return;
    }

    app_files_copy_text(g_files.open_name, sizeof(g_files.open_name),
                        g_files.files[(uint8_t)g_files.selected_file].name);
    g_files.delete_armed = 0U;
    app_files_request_content(0U);
}

static void app_files_request_content(uint32_t offset)
{
    app_storage_request_t request;

    memset(&request, 0, sizeof(request));
    request.operation = APP_STORAGE_OP_READ;
    request.data_offset = offset;
    app_files_copy_text(request.name, sizeof(request.name), g_files.open_name);
    app_files_submit(&request, "READING FILE FROM SD CARD");
}

static void app_files_set_content_status(void)
{
    uint32_t first_byte;
    uint32_t last_byte;

    if (g_files.content_length == 0U)
    {
        app_files_set_status("EMPTY FILE");
        return;
    }

    first_byte = g_files.content_offset + 1U;
    last_byte = g_files.content_offset + g_files.content_length;
    if (g_files.open_file_size >= APP_STORAGE_CONTENT_SIZE)
    {
        (void)snprintf(g_files.status, sizeof(g_files.status),
                       "BYTES %lu-%lu / %lu - SCROLL",
                       (unsigned long)first_byte,
                       (unsigned long)last_byte,
                       (unsigned long)g_files.open_file_size);
    }
    else
    {
        app_files_set_status(g_files.read_only ?
                             "VIEW ONLY - FILE IS BINARY" :
                             "FILE READ FROM SD CARD");
    }
}

/**
 * @brief app_files_save：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
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

/**
 * @brief app_files_edit：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
/**
 * @brief app_files_delete_name：将输入值转换为调用方所需的数据格式或表示形式。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param name 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
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

/**
 * @brief app_files_error_text：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param result 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
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

/**
 * @brief app_files_select_focus：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
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

/**
 * @brief app_files_handle_response：解析并处理当前事件或数据，根据结果推进模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param response 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
static void app_files_handle_response(const app_storage_response_t *response)
{
    if (response->request_id != g_files.pending_request_id ||
        response->operation != g_files.pending_operation)
    {
        /* Response belongs to a previous FILES page instance. */
        return;
    }
    g_files.pending_request_id = 0U;
    g_files.busy = 0U;
    if (!g_files.active)
    {
        return;
    }
    if (response->result != APP_STORAGE_RESULT_OK)
    {
        if (response->operation == APP_STORAGE_OP_CREATE)
        {
            g_files.editor = APP_FILES_EDITOR_NEW_NAME;
        }
        else if (response->operation == APP_STORAGE_OP_WRITE)
        {
            g_files.leave_after_save = 0U;
            g_files.exit_after_save = 0U;
        }
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
            if (g_files.open_after_list)
            {
                g_files.open_after_list = 0U;
                g_files.after_list_status[0] = '\0';
                app_files_read_selected();
                break;
            }
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
            g_files.open_after_list = 1U;
            g_files.edit_after_read = 1U;
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
            g_files.content_offset = response->data_offset;
            g_files.dirty = 0U;
            g_files.delete_armed = 0U;
            g_files.read_only = (!app_files_is_text_file(response->name) ||
                                 response->file_size >= APP_STORAGE_CONTENT_SIZE) ? 1U : 0U;
            g_files.view = APP_FILES_VIEW_CONTENT;
            app_files_set_content_status();
            app_logs_add(APP_LOG_LEVEL_INFO, "FILES", "FILE OPENED");
            if (g_files.edit_after_read)
            {
                g_files.edit_after_read = 0U;
                app_files_start_content_editor();
            }
            else
            {
                app_files_redraw();
            }
            break;

        case APP_STORAGE_OP_WRITE:
            g_files.dirty = 0U;
            g_files.open_file_size = response->content_length;
            if (g_files.leave_after_save)
            {
                g_files.leave_after_save = 0U;
                g_files.view = APP_FILES_VIEW_LIST;
                g_files.open_name[0] = '\0';
                g_files.content[0] = '\0';
                app_files_request_list("FILE SAVED - RETURNED TO LIST");
                break;
            }
            if (g_files.exit_after_save)
            {
                g_files.exit_after_save = 0U;
                g_files.close_requested = 1U;
                app_logs_add(APP_LOG_LEVEL_INFO, "FILES", "FILE SAVED");
                break;
            }
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

        case APP_STORAGE_OP_RENAME:
            app_files_copy_text(g_files.open_name, sizeof(g_files.open_name),
                                response->name);
            g_files.editor = APP_FILES_EDITOR_NONE;
            app_files_copy_text(g_files.focus_name, sizeof(g_files.focus_name),
                                response->name);
            app_files_request_list("FILE RENAMED");
            break;

        default:
            app_files_set_status("UNEXPECTED STORAGE RESPONSE");
            app_files_redraw();
            break;
    }
}

/**
 * @brief app_files_open：启动或启用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
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

/**
 * @brief app_files_close：停止或禁用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_files_close(void)
{
    g_files.active = 0U;
}

static void app_files_show_leave_confirmation(uint8_t exit_application)
{
    g_files.editor = APP_FILES_EDITOR_CONFIRM;
    g_files.confirm_exit = exit_application ? 1U : 0U;
    app_files_editor_redraw();
}

static void app_files_discard_and_leave(void)
{
    g_files.dirty = 0U;
    g_files.editor = APP_FILES_EDITOR_NONE;
    if (g_files.confirm_exit)
    {
        g_files.confirm_exit = 0U;
        g_files.close_requested = 1U;
        return;
    }
    g_files.view = APP_FILES_VIEW_LIST;
    g_files.open_name[0] = '\0';
    g_files.content[0] = '\0';
    g_files.content_length = 0U;
    app_files_set_status("CHANGES DISCARDED");
    app_files_redraw();
}

uint8_t app_files_handle_back(void)
{
    if (!g_files.active)
    {
        return 0U;
    }
    if (g_files.busy)
    {
        app_files_set_status("WAIT FOR STORAGE OPERATION");
        app_files_redraw();
        return 1U;
    }
    if (g_files.editor == APP_FILES_EDITOR_CONFIRM)
    {
        g_files.editor = APP_FILES_EDITOR_NONE;
        g_files.confirm_exit = 0U;
        app_files_set_status("RETURN CANCELED");
        app_files_redraw();
        return 1U;
    }
    if (g_files.editor != APP_FILES_EDITOR_NONE)
    {
        app_files_editor_cancel();
        return 1U;
    }
    if (g_files.view == APP_FILES_VIEW_CONTENT && g_files.dirty)
    {
        app_files_show_leave_confirmation(1U);
        return 1U;
    }
    return 0U;
}

uint8_t app_files_take_close_request(void)
{
    uint8_t requested;

    requested = g_files.close_requested;
    g_files.close_requested = 0U;
    return requested;
}

/**
 * @brief app_files_handle_event：解析并处理当前事件或数据，根据结果推进模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param event 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_files_handle_event(const app_input_event_t *event)
{
    app_ui_files_action_t action;
    app_ui_file_confirm_action_t confirm_action;
    app_ui_keyboard_hit_t keyboard_hit;
    int8_t row;
    uint8_t index;
    uint8_t page_count;

    if (!g_files.active || event == NULL || g_files.busy)
    {
        return;
    }

    if (g_files.editor == APP_FILES_EDITOR_CONFIRM)
    {
        if (event->type != APP_INPUT_EVENT_DOWN)
        {
            return;
        }
        confirm_action = app_ui_file_confirmation_action_at(event->x, event->y);
        if (confirm_action == APP_UI_FILE_CONFIRM_SAVE)
        {
            g_files.editor = APP_FILES_EDITOR_NONE;
            if (g_files.confirm_exit) g_files.exit_after_save = 1U;
            else g_files.leave_after_save = 1U;
            g_files.confirm_exit = 0U;
            app_files_save();
            if (!g_files.busy)
            {
                g_files.exit_after_save = 0U;
                g_files.leave_after_save = 0U;
            }
        }
        else if (confirm_action == APP_UI_FILE_CONFIRM_DISCARD)
        {
            app_files_discard_and_leave();
        }
        else if (confirm_action == APP_UI_FILE_CONFIRM_CANCEL)
        {
            g_files.editor = APP_FILES_EDITOR_NONE;
            g_files.confirm_exit = 0U;
            app_files_set_status("RETURN CANCELED");
            app_files_redraw();
        }
        return;
    }

    if (g_files.editor != APP_FILES_EDITOR_NONE)
    {
        if (event->type == APP_INPUT_EVENT_DOWN)
        {
            keyboard_hit = app_ui_keyboard_hit_test(g_files.keyboard_mode,
                                                    event->x, event->y);
            app_files_editor_handle_key(&keyboard_hit);
        }
        return;
    }

    if (event->type == APP_INPUT_EVENT_SCROLL)
    {
        if (g_files.view == APP_FILES_VIEW_CONTENT)
        {
            uint32_t next_offset;

            if (event->wheel < 0 &&
                g_files.content_offset + g_files.content_length <
                    g_files.open_file_size)
            {
                next_offset = g_files.content_offset + g_files.content_length;
                app_files_request_content(next_offset);
            }
            else if (event->wheel > 0 && g_files.content_offset > 0U)
            {
                next_offset = (g_files.content_offset >=
                               APP_STORAGE_CONTENT_SIZE - 1U) ?
                              g_files.content_offset -
                                  (APP_STORAGE_CONTENT_SIZE - 1U) : 0U;
                app_files_request_content(next_offset);
            }
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
            app_files_start_content_editor();
        }
        else if (action == APP_UI_FILES_ACTION_SAVE)
        {
            app_files_save();
        }
        else if (action == APP_UI_FILES_ACTION_DELETE)
        {
            app_files_delete_name(g_files.open_name);
        }
        else if (action == APP_UI_FILES_ACTION_RENAME)
        {
            app_files_start_name_editor(APP_FILES_EDITOR_RENAME);
        }
        else if (action == APP_UI_FILES_ACTION_LIST)
        {
            if (g_files.dirty)
            {
                app_files_show_leave_confirmation(0U);
            }
            else
            {
                g_files.view = APP_FILES_VIEW_LIST;
                g_files.delete_armed = 0U;
                app_files_set_status("FILE LIST");
                app_files_redraw();
            }
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

/**
 * @brief app_files_update：使用最新数据更新缓存、硬件输出或界面显示状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_files_update(void)
{
    static app_storage_response_t response;

    while (app_storage_receive(&response) == pdPASS)
    {
        app_files_handle_response(&response);
    }
}
