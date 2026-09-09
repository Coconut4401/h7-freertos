#include "app_draw.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "app_logs.h"
#include "app_storage.h"
#include "app_ui.h"
#include "./BSP/LCD/lcd.h"

#define APP_DRAW_LEGACY_NAME       "DRAWING.DRW"
#define APP_DRAW_FILE_VERSION      1U
#define APP_DRAW_MAX_POINTS        1024U
#define APP_DRAW_MAX_SAMPLES       8U

typedef struct { uint16_t x, y, color; uint8_t start, reserved; } app_draw_point_t;
typedef struct
{
    uint8_t magic[4];
    uint16_t version, point_size;
    uint32_t point_count, payload_crc;
} app_draw_file_header_t;
typedef struct
{
    app_draw_file_header_t header;
    app_draw_point_t points[APP_DRAW_MAX_POINTS];
} app_draw_document_t;
typedef enum { APP_DRAW_VIEW_CANVAS = 0, APP_DRAW_VIEW_SAMPLES,
               APP_DRAW_VIEW_RENAME } app_draw_view_t;
typedef enum { APP_DRAW_DIALOG_SAVE = 0, APP_DRAW_DIALOG_OPEN } app_draw_dialog_t;

typedef struct
{
    uint8_t active, busy, drawing, dirty, sample_count;
    int8_t selected_sample;
    app_draw_view_t view;
    app_draw_dialog_t dialog;
    app_ui_keyboard_mode_t keyboard_mode;
    uint16_t selected_color, last_x, last_y, editor_length;
    uint32_t pending_request_id;
    uint32_t point_count;
    char current_name[APP_STORAGE_NAME_LENGTH];
    char pending_name[APP_STORAGE_NAME_LENGTH];
    char rename_source[APP_STORAGE_NAME_LENGTH];
    char editor_buffer[APP_STORAGE_NAME_LENGTH];
    char samples[APP_DRAW_MAX_SAMPLES][APP_STORAGE_NAME_LENGTH];
    char status[64];
    app_draw_point_t points[APP_DRAW_MAX_POINTS];
} app_draw_state_t;

static app_draw_state_t g_draw;
/* The token below outlives page state and protects the asynchronous pointer. */
static app_draw_document_t g_io_document;
static uint32_t g_draw_next_request_id;
static uint32_t g_draw_buffer_request_id;

static void app_draw_copy_text(char *destination, uint32_t size,
                               const char *source)
{
    uint32_t index = 0U;
    if (destination == NULL || size == 0U) return;
    if (source != NULL)
        while (source[index] != '\0' && index + 1U < size)
        { destination[index] = source[index]; index++; }
    destination[index] = '\0';
}

static void app_draw_set_status(const char *status)
{ app_draw_copy_text(g_draw.status, sizeof(g_draw.status), status); }

static uint8_t app_draw_point_is_inside(uint16_t x, uint16_t y)
{
    return (x >= APP_UI_DRAW_CANVAS_LEFT && x <= APP_UI_DRAW_CANVAS_RIGHT &&
            y >= APP_UI_DRAW_CANVAS_TOP && y <= APP_UI_DRAW_CANVAS_BOTTOM) ? 1U : 0U;
}

static uint8_t app_draw_color_is_valid(uint16_t color)
{
    return (color == BLACK || color == RED || color == GREEN || color == BLUE) ? 1U : 0U;
}

static uint32_t app_draw_crc32(const void *data, uint32_t length)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFU, index;
    uint8_t bit;
    for (index = 0U; index < length; index++)
    {
        crc ^= bytes[index];
        for (bit = 0U; bit < 8U; bit++)
            crc = (crc & 1U) ? ((crc >> 1U) ^ 0xEDB88320U) : (crc >> 1U);
    }
    return crc ^ 0xFFFFFFFFU;
}

static uint8_t app_draw_name_equal(const char *first, const char *second)
{
    char a, b;
    uint8_t index = 0U;
    do
    {
        a = first[index]; b = second[index];
        if (a >= 'a' && a <= 'z') a = (char)(a - ('a' - 'A'));
        if (b >= 'a' && b <= 'z') b = (char)(b - ('a' - 'A'));
        if (a != b) return 0U;
        index++;
    } while (a != '\0' && index < APP_STORAGE_NAME_LENGTH);
    return 1U;
}

static uint8_t app_draw_name_exists(const char *name)
{
    uint8_t index;
    for (index = 0U; index < g_draw.sample_count; index++)
        if (app_draw_name_equal(g_draw.samples[index], name)) return 1U;
    return 0U;
}

static uint8_t app_draw_name_finalize(void)
{
    uint16_t length = g_draw.editor_length;
    const char *dot = strchr(g_draw.editor_buffer, '.');
    uint8_t base = 0U, extension = 0U, index;
    if (dot == NULL)
    {
        if (length == 0U || length > 8U || length + 4U >= APP_STORAGE_NAME_LENGTH) return 0U;
        memcpy(&g_draw.editor_buffer[length], ".DRW", 5U);
        length = (uint16_t)(length + 4U); g_draw.editor_length = length;
    }
    for (index = 0U; index < length; index++)
    {
        char character = g_draw.editor_buffer[index];
        if (character == '.')
        { if (base == 0U || extension != 0U) return 0U; extension = 1U; }
        else if (!((character >= 'A' && character <= 'Z') ||
                   (character >= 'a' && character <= 'z') ||
                   (character >= '0' && character <= '9') || character == '_' ||
                   character == '-')) return 0U;
        else if (extension) extension++; else base++;
    }
    dot = strrchr(g_draw.editor_buffer, '.');
    return (base > 0U && base <= 8U && extension == 4U && dot != NULL &&
            (dot[1] == 'D' || dot[1] == 'd') &&
            (dot[2] == 'R' || dot[2] == 'r') &&
            (dot[3] == 'W' || dot[3] == 'w')) ? 1U : 0U;
}

static void app_draw_replay(void)
{
    uint32_t index;
    for (index = 0U; index < g_draw.point_count; index++)
    {
        const app_draw_point_t *point = &g_draw.points[index];
        if (point->start || index == 0U)
            app_ui_draw_stroke(point->x, point->y, point->x, point->y, point->color);
        else
        {
            const app_draw_point_t *previous = &g_draw.points[index - 1U];
            app_ui_draw_stroke(previous->x, previous->y, point->x, point->y, point->color);
        }
    }
}

static void app_draw_redraw(void)
{
    if (!g_draw.active) return;
    if (g_draw.view == APP_DRAW_VIEW_SAMPLES)
        app_ui_show_draw_save_dialog(g_draw.samples, g_draw.sample_count,
                                     g_draw.selected_sample, g_draw.status);
    else if (g_draw.view == APP_DRAW_VIEW_RENAME)
        app_ui_show_file_editor("RENAME DRAWING", g_draw.editor_buffer, "",
                                g_draw.editor_length, 1U, g_draw.status,
                                g_draw.keyboard_mode);
    else
    {
        app_ui_show_draw(g_draw.selected_color, g_draw.status, g_draw.busy);
        app_draw_replay();
    }
}

static void app_draw_update_controls(void)
{
    if (!g_draw.active) return;
    if (g_draw.view == APP_DRAW_VIEW_CANVAS)
        app_ui_update_draw_controls(g_draw.selected_color, g_draw.status, g_draw.busy);
    else app_draw_redraw();
}

static uint8_t app_draw_submit(app_storage_request_t *request, const char *status)
{
    if (g_draw.busy) return 0U;
    if (request->binary_data != NULL && g_draw_buffer_request_id != 0U)
    {
        app_draw_set_status("FINISHING PREVIOUS STORAGE OPERATION");
        app_draw_update_controls();
        return 0U;
    }
    g_draw_next_request_id++;
    if (g_draw_next_request_id == 0U) g_draw_next_request_id = 1U;
    request->request_id = g_draw_next_request_id;
    if (app_storage_submit(request) != pdPASS)
    {
        app_draw_set_status("STORAGE REQUEST QUEUE IS FULL");
        app_draw_update_controls(); return 0U;
    }
    g_draw.pending_request_id = request->request_id;
    if (request->binary_data != NULL)
        g_draw_buffer_request_id = request->request_id;
    g_draw.busy = 1U; g_draw.drawing = 0U;
    app_draw_set_status(status); app_draw_update_controls(); return 1U;
}

static uint8_t app_draw_append_point(uint16_t x, uint16_t y, uint8_t start)
{
    app_draw_point_t *point;
    if (g_draw.point_count >= APP_DRAW_MAX_POINTS)
    {
        g_draw.drawing = 0U;
        app_logs_add(APP_LOG_LEVEL_WARNING, "DRAW", "POINT BUFFER FULL");
        app_draw_set_status("POINT BUFFER FULL - SAVE OR UNDO");
        app_draw_update_controls(); return 0U;
    }
    point = &g_draw.points[g_draw.point_count++];
    point->x = x; point->y = y; point->color = g_draw.selected_color;
    point->start = start; point->reserved = 0U; g_draw.dirty = 1U;
    return 1U;
}

static void app_draw_undo(void)
{
    uint32_t start;
    if (g_draw.point_count == 0U)
    { app_draw_set_status("NOTHING TO UNDO"); app_draw_update_controls(); return; }
    start = g_draw.point_count - 1U;
    while (start > 0U && !g_draw.points[start].start) start--;
    g_draw.point_count = start; g_draw.drawing = 0U; g_draw.dirty = 1U;
    app_draw_set_status("LAST STROKE UNDONE"); app_draw_redraw();
}

static void app_draw_submit_list(app_draw_dialog_t dialog)
{
    app_storage_request_t request;
    memset(&request, 0, sizeof(request)); request.operation = APP_STORAGE_OP_DRAW_LIST;
    g_draw.dialog = dialog; g_draw.view = APP_DRAW_VIEW_SAMPLES;
    g_draw.selected_sample = -1;
    app_draw_submit(&request, "READING DRAWING SAMPLES");
}

static void app_draw_submit_save(const char *name)
{
    app_storage_request_t request;
    uint32_t payload_length;
    if (g_draw_buffer_request_id != 0U)
    {
        app_draw_set_status("FINISHING PREVIOUS STORAGE OPERATION");
        app_draw_update_controls();
        return;
    }
    memcpy(g_io_document.header.magic, "DRW1", 4U);
    g_io_document.header.version = APP_DRAW_FILE_VERSION;
    g_io_document.header.point_size = (uint16_t)sizeof(app_draw_point_t);
    g_io_document.header.point_count = g_draw.point_count;
    payload_length = g_draw.point_count * (uint32_t)sizeof(app_draw_point_t);
    memcpy(g_io_document.points, g_draw.points, payload_length);
    g_io_document.header.payload_crc = app_draw_crc32(g_io_document.points, payload_length);
    memset(&request, 0, sizeof(request)); request.operation = APP_STORAGE_OP_WRITE_BINARY;
    app_draw_copy_text(request.name, sizeof(request.name), name);
    request.binary_data = &g_io_document;
    request.binary_length = (uint32_t)sizeof(app_draw_file_header_t) + payload_length;
    app_draw_copy_text(g_draw.pending_name, sizeof(g_draw.pending_name), name);
    app_draw_submit(&request, "SAVING DRAWING SAMPLE");
}

static void app_draw_submit_open(const char *name)
{
    app_storage_request_t request;
    memset(&request, 0, sizeof(request)); request.operation = APP_STORAGE_OP_READ_BINARY;
    app_draw_copy_text(request.name, sizeof(request.name), name);
    request.binary_data = &g_io_document; request.binary_capacity = sizeof(g_io_document);
    app_draw_copy_text(g_draw.pending_name, sizeof(g_draw.pending_name), name);
    app_draw_submit(&request, "OPENING DRAWING SAMPLE");
}

static void app_draw_make_new_name(char name[APP_STORAGE_NAME_LENGTH])
{
    uint16_t number;
    for (number = 1U; number < 10000U; number++)
    {
        name[0] = 'D'; name[1] = 'R'; name[2] = 'A'; name[3] = 'W';
        name[4] = (char)('0' + (number / 1000U) % 10U);
        name[5] = (char)('0' + (number / 100U) % 10U);
        name[6] = (char)('0' + (number / 10U) % 10U);
        name[7] = (char)('0' + number % 10U); memcpy(&name[8], ".DRW", 5U);
        if (!app_draw_name_exists(name)) return;
    }
    app_draw_copy_text(name, APP_STORAGE_NAME_LENGTH, APP_DRAW_LEGACY_NAME);
}

static void app_draw_start_rename(void)
{
    const char *dot;
    uint16_t length;
    if (g_draw.selected_sample < 0 || (uint8_t)g_draw.selected_sample >= g_draw.sample_count)
    { app_draw_set_status("SELECT A SAMPLE FIRST"); app_draw_redraw(); return; }
    app_draw_copy_text(g_draw.rename_source, sizeof(g_draw.rename_source),
                       g_draw.samples[(uint8_t)g_draw.selected_sample]);
    dot = strrchr(g_draw.rename_source, '.');
    length = dot ? (uint16_t)(dot - g_draw.rename_source) :
                   (uint16_t)strlen(g_draw.rename_source);
    memcpy(g_draw.editor_buffer, g_draw.rename_source, length);
    g_draw.editor_buffer[length] = '\0'; g_draw.editor_length = length;
    g_draw.keyboard_mode = APP_UI_KEYBOARD_MODE_UPPER;
    g_draw.view = APP_DRAW_VIEW_RENAME;
    app_draw_set_status("TYPE NAME THEN PRESS OK"); app_draw_redraw();
}

static void app_draw_submit_rename(void)
{
    app_storage_request_t request;
    if (!app_draw_name_finalize() ||
        (!app_draw_name_equal(g_draw.rename_source, g_draw.editor_buffer) &&
         app_draw_name_exists(g_draw.editor_buffer)))
    { app_draw_set_status("INVALID OR DUPLICATE DRAW NAME"); app_draw_redraw(); return; }
    if (app_draw_name_equal(g_draw.rename_source, g_draw.editor_buffer))
    { g_draw.view = APP_DRAW_VIEW_SAMPLES; app_draw_set_status("NAME UNCHANGED"); app_draw_redraw(); return; }
    memset(&request, 0, sizeof(request)); request.operation = APP_STORAGE_OP_DRAW_RENAME;
    app_draw_copy_text(request.name, sizeof(request.name), g_draw.rename_source);
    app_draw_copy_text(request.new_name, sizeof(request.new_name), g_draw.editor_buffer);
    app_draw_copy_text(g_draw.pending_name, sizeof(g_draw.pending_name), g_draw.editor_buffer);
    app_draw_submit(&request, "RENAMING DRAWING SAMPLE");
}

static uint8_t app_draw_document_is_valid(uint32_t data_length)
{
    uint32_t index, payload_length;
    if (sizeof(app_draw_file_header_t) != 16U || sizeof(app_draw_point_t) != 8U ||
        data_length < sizeof(app_draw_file_header_t) ||
        memcmp(g_io_document.header.magic, "DRW1", 4U) != 0 ||
        g_io_document.header.version != APP_DRAW_FILE_VERSION ||
        g_io_document.header.point_size != sizeof(app_draw_point_t) ||
        g_io_document.header.point_count > APP_DRAW_MAX_POINTS) return 0U;
    payload_length = g_io_document.header.point_count * (uint32_t)sizeof(app_draw_point_t);
    if (data_length != sizeof(app_draw_file_header_t) + payload_length ||
        app_draw_crc32(g_io_document.points, payload_length) !=
        g_io_document.header.payload_crc) return 0U;
    for (index = 0U; index < g_io_document.header.point_count; index++)
    {
        const app_draw_point_t *point = &g_io_document.points[index];
        if (!app_draw_point_is_inside(point->x, point->y) ||
            !app_draw_color_is_valid(point->color) || point->start > 1U ||
            (index == 0U && point->start == 0U)) return 0U;
    }
    return 1U;
}

static const char *app_draw_storage_error(app_storage_result_t result)
{
    if (result == APP_STORAGE_RESULT_NOT_READY) return "SD CARD OR FAT32 IS NOT READY";
    if (result == APP_STORAGE_RESULT_NOT_FOUND) return "DRAWING SAMPLE WAS NOT FOUND";
    if (result == APP_STORAGE_RESULT_EXISTS) return "DRAWING NAME ALREADY EXISTS";
    if (result == APP_STORAGE_RESULT_INVALID_NAME) return "INVALID DRAWING FILE NAME";
    if (result == APP_STORAGE_RESULT_QUEUE_FULL) return "STORAGE REQUEST QUEUE IS FULL";
    return "DRAW FILE OPERATION FAILED";
}

void app_draw_open(void)
{
    memset(&g_draw, 0, sizeof(g_draw));
    g_draw.active = 1U;
    g_draw.selected_sample = -1; g_draw.selected_color = BLACK;
    g_draw.view = APP_DRAW_VIEW_CANVAS;
    app_draw_set_status("DRAW WITH TOUCH - SAVE SUPPORTS 8 SAMPLES"); app_draw_redraw();
}

void app_draw_close(void) { g_draw.active = 0U; g_draw.drawing = 0U; }

uint8_t app_draw_handle_back(void)
{
    if (!g_draw.active) return 0U;
    if (g_draw.busy)
    { app_draw_set_status("WAIT FOR STORAGE OPERATION"); app_draw_update_controls(); return 1U; }
    if (g_draw.view != APP_DRAW_VIEW_CANVAS)
    {
        g_draw.view = APP_DRAW_VIEW_CANVAS;
        app_draw_set_status("SAMPLE DIALOG CANCELED"); app_draw_redraw(); return 1U;
    }
    return 0U;
}

static void app_draw_handle_keyboard(const app_ui_keyboard_hit_t *hit)
{
    char character;
    uint8_t full_redraw = 0U;
    if (hit->action == APP_UI_KEYBOARD_ACTION_SHIFT)
    { g_draw.keyboard_mode = (g_draw.keyboard_mode == APP_UI_KEYBOARD_MODE_UPPER) ?
        APP_UI_KEYBOARD_MODE_LOWER : APP_UI_KEYBOARD_MODE_UPPER; full_redraw = 1U; }
    else if (hit->action == APP_UI_KEYBOARD_ACTION_NUMBER)
    { g_draw.keyboard_mode = (g_draw.keyboard_mode == APP_UI_KEYBOARD_MODE_NUMBER) ?
        APP_UI_KEYBOARD_MODE_LOWER : APP_UI_KEYBOARD_MODE_NUMBER; full_redraw = 1U; }
    else if (hit->action == APP_UI_KEYBOARD_ACTION_BACKSPACE)
    { if (g_draw.editor_length > 0U) g_draw.editor_buffer[--g_draw.editor_length] = '\0'; }
    else if (hit->action == APP_UI_KEYBOARD_ACTION_CHAR)
    {
        character = hit->character;
        if (g_draw.editor_length + 1U < APP_STORAGE_NAME_LENGTH &&
            ((character >= 'A' && character <= 'Z') ||
             (character >= 'a' && character <= 'z') ||
             (character >= '0' && character <= '9') || character == '_' || character == '-'))
        { g_draw.editor_buffer[g_draw.editor_length++] = character;
          g_draw.editor_buffer[g_draw.editor_length] = '\0'; }
    }
    else if (hit->action == APP_UI_KEYBOARD_ACTION_OK) { app_draw_submit_rename(); return; }
    else if (hit->action == APP_UI_KEYBOARD_ACTION_CANCEL)
    { g_draw.view = APP_DRAW_VIEW_SAMPLES; app_draw_set_status("RENAME CANCELED"); app_draw_redraw(); return; }
    if (full_redraw) app_draw_redraw();
    else app_ui_update_file_editor("RENAME DRAWING", g_draw.editor_buffer, "",
        g_draw.editor_length, 1U, g_draw.status, g_draw.keyboard_mode);
}

static void app_draw_handle_sample_dialog(const app_input_event_t *event)
{
    app_ui_draw_save_action_t action;
    uint8_t index;
    char name[APP_STORAGE_NAME_LENGTH];
    if (event->type != APP_INPUT_EVENT_DOWN) return;
    action = app_ui_draw_save_action_at(event->x, event->y);
    if (action >= APP_UI_DRAW_SAVE_SAMPLE0 && action <= APP_UI_DRAW_SAVE_SAMPLE7)
    {
        index = (uint8_t)(action - APP_UI_DRAW_SAVE_SAMPLE0);
        if (index >= g_draw.sample_count) return;
        if (g_draw.selected_sample == (int8_t)index)
        {
            if (g_draw.dialog == APP_DRAW_DIALOG_SAVE) app_draw_submit_save(g_draw.samples[index]);
            else app_draw_submit_open(g_draw.samples[index]);
        }
        else
        {
            g_draw.selected_sample = (int8_t)index;
            app_draw_set_status(g_draw.dialog == APP_DRAW_DIALOG_SAVE ?
                "TOUCH SELECTED SAMPLE AGAIN TO OVERWRITE" :
                "TOUCH SELECTED SAMPLE AGAIN TO OPEN"); app_draw_redraw();
        }
    }
    else if (action == APP_UI_DRAW_SAVE_NEW && g_draw.dialog == APP_DRAW_DIALOG_SAVE)
    {
        if (g_draw.sample_count >= APP_DRAW_MAX_SAMPLES)
        { app_draw_set_status("8 SAMPLE LIMIT - RENAME OR OVERWRITE ONE"); app_draw_redraw(); return; }
        app_draw_make_new_name(name); app_draw_submit_save(name);
    }
    else if (action == APP_UI_DRAW_SAVE_RENAME) app_draw_start_rename();
    else if (action == APP_UI_DRAW_SAVE_CANCEL)
    { g_draw.view = APP_DRAW_VIEW_CANVAS; app_draw_set_status("SAMPLE DIALOG CANCELED"); app_draw_redraw(); }
}

void app_draw_handle_event(const app_input_event_t *event)
{
    app_ui_draw_action_t action;
    if (!g_draw.active || event == NULL || g_draw.busy) return;
    if (g_draw.view == APP_DRAW_VIEW_RENAME)
    {
        if (event->type == APP_INPUT_EVENT_DOWN)
        {
            app_ui_keyboard_hit_t hit = app_ui_keyboard_hit_test(g_draw.keyboard_mode,
                                                                  event->x, event->y);
            app_draw_handle_keyboard(&hit);
        }
        return;
    }
    if (g_draw.view == APP_DRAW_VIEW_SAMPLES) { app_draw_handle_sample_dialog(event); return; }
    if (event->type == APP_INPUT_EVENT_UP) { g_draw.drawing = 0U; return; }
    if (event->type == APP_INPUT_EVENT_DOWN)
    {
        action = app_ui_draw_action_at(event->x, event->y);
        if (action == APP_UI_DRAW_ACTION_UNDO) { app_draw_undo(); return; }
        if (action == APP_UI_DRAW_ACTION_SAVE) { app_draw_submit_list(APP_DRAW_DIALOG_SAVE); return; }
        if (action == APP_UI_DRAW_ACTION_OPEN) { app_draw_submit_list(APP_DRAW_DIALOG_OPEN); return; }
        if (action == APP_UI_DRAW_ACTION_BLACK) g_draw.selected_color = BLACK;
        else if (action == APP_UI_DRAW_ACTION_RED) g_draw.selected_color = RED;
        else if (action == APP_UI_DRAW_ACTION_GREEN) g_draw.selected_color = GREEN;
        else if (action == APP_UI_DRAW_ACTION_BLUE) g_draw.selected_color = BLUE;
        if (action >= APP_UI_DRAW_ACTION_BLACK && action <= APP_UI_DRAW_ACTION_BLUE)
        { app_draw_set_status("BRUSH COLOR SELECTED"); app_draw_update_controls(); return; }
        if (app_draw_point_is_inside(event->x, event->y) &&
            app_draw_append_point(event->x, event->y, 1U))
        {
            g_draw.drawing = 1U; g_draw.last_x = event->x; g_draw.last_y = event->y;
            app_ui_draw_stroke(event->x, event->y, event->x, event->y, g_draw.selected_color);
        }
        return;
    }
    if (event->type == APP_INPUT_EVENT_MOVE && g_draw.drawing)
    {
        if (!app_draw_point_is_inside(event->x, event->y)) { g_draw.drawing = 0U; return; }
        if (event->x == g_draw.last_x && event->y == g_draw.last_y) return;
        if (app_draw_append_point(event->x, event->y, 0U))
        {
            app_ui_draw_stroke(g_draw.last_x, g_draw.last_y, event->x, event->y,
                               g_draw.selected_color);
            g_draw.last_x = event->x; g_draw.last_y = event->y;
        }
    }
}

void app_draw_update(void)
{
    app_storage_binary_response_t response;
    while (app_storage_receive_binary(&response) == pdPASS)
    {
        uint32_t payload_length;
        if (response.request_id == g_draw_buffer_request_id)
            g_draw_buffer_request_id = 0U;
        if (response.request_id != g_draw.pending_request_id)
        {
            /* Response belongs to a closed/reopened DRAW instance. */
            continue;
        }
        g_draw.pending_request_id = 0U;
        g_draw.busy = 0U;
        if (!g_draw.active) continue;
        if (response.result != APP_STORAGE_RESULT_OK)
        {
            app_logs_add(APP_LOG_LEVEL_ERROR, "DRAW", "SD OPERATION FAILED");
            app_draw_set_status(app_draw_storage_error(response.result));
            app_draw_update_controls(); continue;
        }
        if (response.operation == APP_STORAGE_OP_DRAW_LIST)
        {
            g_draw.sample_count = response.draw_count;
            memcpy(g_draw.samples, response.draw_names, sizeof(g_draw.samples));
            g_draw.selected_sample = -1;
            app_draw_set_status(g_draw.dialog == APP_DRAW_DIALOG_SAVE ?
                "SELECT TWICE TO OVERWRITE, OR SAVE NEW" :
                "SELECT A SAMPLE TWICE TO OPEN"); app_draw_redraw();
        }
        else if (response.operation == APP_STORAGE_OP_WRITE_BINARY)
        {
            app_draw_copy_text(g_draw.current_name, sizeof(g_draw.current_name), g_draw.pending_name);
            g_draw.dirty = 0U; g_draw.view = APP_DRAW_VIEW_CANVAS;
            app_logs_add(APP_LOG_LEVEL_INFO, "DRAW", "DRAWING SAVED");
            app_draw_set_status("DRAWING SAMPLE SAVED TO SD CARD"); app_draw_redraw();
        }
        else if (response.operation == APP_STORAGE_OP_READ_BINARY)
        {
            if (!app_draw_document_is_valid(response.data_length))
            {
                app_logs_add(APP_LOG_LEVEL_ERROR, "DRAW", "DRAW FILE CRC ERROR");
                app_draw_set_status("INVALID DRAW FILE OR CRC ERROR");
                app_draw_update_controls(); continue;
            }
            g_draw.point_count = g_io_document.header.point_count;
            payload_length = g_draw.point_count * (uint32_t)sizeof(app_draw_point_t);
            memcpy(g_draw.points, g_io_document.points, payload_length);
            app_draw_copy_text(g_draw.current_name, sizeof(g_draw.current_name), g_draw.pending_name);
            g_draw.dirty = 0U; g_draw.view = APP_DRAW_VIEW_CANVAS;
            app_logs_add(APP_LOG_LEVEL_INFO, "DRAW", "DRAWING LOADED");
            app_draw_set_status("DRAWING SAMPLE LOADED FROM SD CARD"); app_draw_redraw();
        }
        else if (response.operation == APP_STORAGE_OP_DRAW_RENAME)
        {
            if (app_draw_name_equal(g_draw.current_name, g_draw.rename_source))
                app_draw_copy_text(g_draw.current_name, sizeof(g_draw.current_name), g_draw.pending_name);
            app_logs_add(APP_LOG_LEVEL_INFO, "DRAW", "DRAWING RENAMED");
            app_draw_submit_list(g_draw.dialog);
        }
    }
}
