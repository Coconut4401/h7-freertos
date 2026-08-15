#include "app_draw.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "app_storage.h"
#include "app_ui.h"
#include "./BSP/LCD/lcd.h"

#define APP_DRAW_FILE_NAME       "DRAWING.DRW"
#define APP_DRAW_FILE_VERSION    1U
#define APP_DRAW_MAX_POINTS      1024U

typedef struct
{
    uint16_t x;
    uint16_t y;
    uint16_t color;
    uint8_t start;
    uint8_t reserved;
} app_draw_point_t;

typedef struct
{
    uint8_t magic[4];
    uint16_t version;
    uint16_t point_size;
    uint32_t point_count;
    uint32_t payload_crc;
} app_draw_file_header_t;

typedef struct
{
    app_draw_file_header_t header;
    app_draw_point_t points[APP_DRAW_MAX_POINTS];
} app_draw_document_t;

typedef struct
{
    uint8_t active;
    uint8_t busy;
    uint8_t drawing;
    uint16_t selected_color;
    uint16_t last_x;
    uint16_t last_y;
    uint32_t point_count;
    char status[64];
    app_draw_point_t points[APP_DRAW_MAX_POINTS];
} app_draw_state_t;

static app_draw_state_t g_draw;
static app_draw_document_t g_io_document;

static void app_draw_copy_text(char *destination,
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

static void app_draw_set_status(const char *status)
{
    app_draw_copy_text(g_draw.status, sizeof(g_draw.status), status);
}

static uint8_t app_draw_point_is_inside(uint16_t x, uint16_t y)
{
    return (x >= APP_UI_DRAW_CANVAS_LEFT &&
            x <= APP_UI_DRAW_CANVAS_RIGHT &&
            y >= APP_UI_DRAW_CANVAS_TOP &&
            y <= APP_UI_DRAW_CANVAS_BOTTOM) ? 1U : 0U;
}

static uint8_t app_draw_color_is_valid(uint16_t color)
{
    return (color == BLACK || color == RED ||
            color == GREEN || color == BLUE) ? 1U : 0U;
}

static uint32_t app_draw_crc32(const void *data, uint32_t length)
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

static void app_draw_replay(void)
{
    uint32_t index;
    const app_draw_point_t *point;
    const app_draw_point_t *previous;

    for (index = 0U; index < g_draw.point_count; index++)
    {
        point = &g_draw.points[index];
        if (point->start || index == 0U)
        {
            app_ui_draw_stroke(point->x, point->y,
                               point->x, point->y, point->color);
        }
        else
        {
            previous = &g_draw.points[index - 1U];
            app_ui_draw_stroke(previous->x, previous->y,
                               point->x, point->y, point->color);
        }
    }
}

static void app_draw_redraw(void)
{
    if (!g_draw.active)
    {
        return;
    }
    app_ui_show_draw(g_draw.selected_color, g_draw.status, g_draw.busy);
    app_draw_replay();
}

static void app_draw_update_controls(void)
{
    if (g_draw.active)
    {
        app_ui_update_draw_controls(g_draw.selected_color,
                                    g_draw.status, g_draw.busy);
    }
}

static uint8_t app_draw_append_point(uint16_t x,
                                     uint16_t y,
                                     uint8_t start)
{
    app_draw_point_t *point;

    if (g_draw.point_count >= APP_DRAW_MAX_POINTS)
    {
        g_draw.drawing = 0U;
        app_draw_set_status("POINT BUFFER FULL - SAVE OR CLEAR");
        app_draw_update_controls();
        return 0U;
    }

    point = &g_draw.points[g_draw.point_count];
    point->x = x;
    point->y = y;
    point->color = g_draw.selected_color;
    point->start = start;
    point->reserved = 0U;
    g_draw.point_count++;
    return 1U;
}

static void app_draw_submit_save(void)
{
    app_storage_request_t request;
    uint32_t payload_length;

    g_io_document.header.magic[0] = 'D';
    g_io_document.header.magic[1] = 'R';
    g_io_document.header.magic[2] = 'W';
    g_io_document.header.magic[3] = '1';
    g_io_document.header.version = APP_DRAW_FILE_VERSION;
    g_io_document.header.point_size = (uint16_t)sizeof(app_draw_point_t);
    g_io_document.header.point_count = g_draw.point_count;
    payload_length = g_draw.point_count * (uint32_t)sizeof(app_draw_point_t);
    memcpy(g_io_document.points, g_draw.points, payload_length);
    g_io_document.header.payload_crc =
        app_draw_crc32(g_io_document.points, payload_length);

    memset(&request, 0, sizeof(request));
    request.operation = APP_STORAGE_OP_WRITE_BINARY;
    app_draw_copy_text(request.name, sizeof(request.name), APP_DRAW_FILE_NAME);
    request.binary_data = &g_io_document;
    request.binary_length = (uint32_t)sizeof(app_draw_file_header_t) +
                            payload_length;
    if (app_storage_submit(&request) != pdPASS)
    {
        app_draw_set_status("STORAGE REQUEST QUEUE IS FULL");
        app_draw_update_controls();
        return;
    }

    g_draw.busy = 1U;
    g_draw.drawing = 0U;
    app_draw_set_status("SAVING DRAWING.DRW");
    app_draw_update_controls();
}

static void app_draw_submit_open(void)
{
    app_storage_request_t request;

    memset(&request, 0, sizeof(request));
    request.operation = APP_STORAGE_OP_READ_BINARY;
    app_draw_copy_text(request.name, sizeof(request.name), APP_DRAW_FILE_NAME);
    request.binary_data = &g_io_document;
    request.binary_capacity = sizeof(g_io_document);
    if (app_storage_submit(&request) != pdPASS)
    {
        app_draw_set_status("STORAGE REQUEST QUEUE IS FULL");
        app_draw_update_controls();
        return;
    }

    g_draw.busy = 1U;
    g_draw.drawing = 0U;
    app_draw_set_status("OPENING DRAWING.DRW");
    app_draw_update_controls();
}

static uint8_t app_draw_document_is_valid(uint32_t data_length)
{
    uint32_t index;
    uint32_t payload_length;
    const app_draw_point_t *point;

    if (sizeof(app_draw_file_header_t) != 16U ||
        sizeof(app_draw_point_t) != 8U ||
        data_length < sizeof(app_draw_file_header_t) ||
        g_io_document.header.magic[0] != 'D' ||
        g_io_document.header.magic[1] != 'R' ||
        g_io_document.header.magic[2] != 'W' ||
        g_io_document.header.magic[3] != '1' ||
        g_io_document.header.version != APP_DRAW_FILE_VERSION ||
        g_io_document.header.point_size != sizeof(app_draw_point_t) ||
        g_io_document.header.point_count > APP_DRAW_MAX_POINTS)
    {
        return 0U;
    }

    payload_length = g_io_document.header.point_count *
                     (uint32_t)sizeof(app_draw_point_t);
    if (data_length != sizeof(app_draw_file_header_t) + payload_length ||
        app_draw_crc32(g_io_document.points, payload_length) !=
        g_io_document.header.payload_crc)
    {
        return 0U;
    }

    for (index = 0U; index < g_io_document.header.point_count; index++)
    {
        point = &g_io_document.points[index];
        if (!app_draw_point_is_inside(point->x, point->y) ||
            !app_draw_color_is_valid(point->color) ||
            point->start > 1U || (index == 0U && point->start == 0U))
        {
            return 0U;
        }
    }
    return 1U;
}

static const char *app_draw_storage_error(app_storage_result_t result)
{
    switch (result)
    {
        case APP_STORAGE_RESULT_NOT_READY:
            return "SD CARD OR FAT32 IS NOT READY";

        case APP_STORAGE_RESULT_NOT_FOUND:
            return "DRAWING.DRW WAS NOT FOUND";

        case APP_STORAGE_RESULT_QUEUE_FULL:
            return "STORAGE REQUEST QUEUE IS FULL";

        default:
            return "DRAW FILE OPERATION FAILED";
    }
}

void app_draw_open(void)
{
    memset(&g_draw, 0, sizeof(g_draw));
    g_draw.active = 1U;
    g_draw.selected_color = BLACK;
    app_draw_set_status("DRAW WITH TOUCH - SAVE STORES TO SD");
    app_draw_redraw();
}

void app_draw_close(void)
{
    g_draw.active = 0U;
    g_draw.drawing = 0U;
}

void app_draw_handle_event(const app_input_event_t *event)
{
    app_ui_draw_action_t action;

    if (!g_draw.active || event == NULL || g_draw.busy)
    {
        return;
    }

    if (event->type == APP_INPUT_EVENT_UP)
    {
        g_draw.drawing = 0U;
        return;
    }

    if (event->type == APP_INPUT_EVENT_DOWN)
    {
        action = app_ui_draw_action_at(event->x, event->y);
        if (action == APP_UI_DRAW_ACTION_CLEAR)
        {
            g_draw.point_count = 0U;
            g_draw.drawing = 0U;
            app_draw_set_status("CANVAS CLEARED - NOT SAVED YET");
            app_draw_redraw();
            return;
        }
        if (action == APP_UI_DRAW_ACTION_SAVE)
        {
            app_draw_submit_save();
            return;
        }
        if (action == APP_UI_DRAW_ACTION_OPEN)
        {
            app_draw_submit_open();
            return;
        }
        if (action == APP_UI_DRAW_ACTION_BLACK)
        {
            g_draw.selected_color = BLACK;
            app_draw_set_status("BLACK BRUSH SELECTED");
            app_draw_update_controls();
            return;
        }
        if (action == APP_UI_DRAW_ACTION_RED)
        {
            g_draw.selected_color = RED;
            app_draw_set_status("RED BRUSH SELECTED");
            app_draw_update_controls();
            return;
        }
        if (action == APP_UI_DRAW_ACTION_GREEN)
        {
            g_draw.selected_color = GREEN;
            app_draw_set_status("GREEN BRUSH SELECTED");
            app_draw_update_controls();
            return;
        }
        if (action == APP_UI_DRAW_ACTION_BLUE)
        {
            g_draw.selected_color = BLUE;
            app_draw_set_status("BLUE BRUSH SELECTED");
            app_draw_update_controls();
            return;
        }

        if (app_draw_point_is_inside(event->x, event->y) &&
            app_draw_append_point(event->x, event->y, 1U))
        {
            g_draw.drawing = 1U;
            g_draw.last_x = event->x;
            g_draw.last_y = event->y;
            app_ui_draw_stroke(event->x, event->y,
                               event->x, event->y, g_draw.selected_color);
        }
        return;
    }

    if (event->type == APP_INPUT_EVENT_MOVE && g_draw.drawing)
    {
        if (!app_draw_point_is_inside(event->x, event->y))
        {
            g_draw.drawing = 0U;
            return;
        }
        if (event->x == g_draw.last_x && event->y == g_draw.last_y)
        {
            return;
        }
        if (app_draw_append_point(event->x, event->y, 0U))
        {
            app_ui_draw_stroke(g_draw.last_x, g_draw.last_y,
                               event->x, event->y, g_draw.selected_color);
            g_draw.last_x = event->x;
            g_draw.last_y = event->y;
        }
    }
}

void app_draw_update(void)
{
    app_storage_binary_response_t response;
    uint32_t payload_length;

    while (app_storage_receive_binary(&response) == pdPASS)
    {
        g_draw.busy = 0U;
        if (!g_draw.active)
        {
            continue;
        }
        if (response.result != APP_STORAGE_RESULT_OK)
        {
            app_draw_set_status(app_draw_storage_error(response.result));
            app_draw_update_controls();
            continue;
        }

        if (response.operation == APP_STORAGE_OP_WRITE_BINARY)
        {
            app_draw_set_status("DRAWING.DRW SAVED TO SD CARD");
            app_draw_update_controls();
        }
        else if (response.operation == APP_STORAGE_OP_READ_BINARY)
        {
            if (!app_draw_document_is_valid(response.data_length))
            {
                app_draw_set_status("INVALID DRAW FILE OR CRC ERROR");
                app_draw_update_controls();
                continue;
            }
            g_draw.point_count = g_io_document.header.point_count;
            payload_length = g_draw.point_count *
                             (uint32_t)sizeof(app_draw_point_t);
            memcpy(g_draw.points, g_io_document.points, payload_length);
            app_draw_set_status("DRAWING.DRW LOADED FROM SD CARD");
            app_draw_redraw();
        }
    }
}
