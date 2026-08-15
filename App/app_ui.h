#ifndef APP_UI_H
#define APP_UI_H

#include <stdint.h>

#include "app_logs.h"
#include "app_monitor.h"
#include "app_storage.h"

#define APP_UI_KEY_NONE    (-1)
#define APP_UI_KEY_CLEAR   10
#define APP_UI_KEY_ENTER   11

typedef enum
{
    APP_UI_APP_FILES = 0,
    APP_UI_APP_DRAW,
    APP_UI_APP_MUSIC,
    APP_UI_APP_LOGS,
    APP_UI_APP_MONITOR,
    APP_UI_APP_SETTINGS,
    APP_UI_APP_COUNT
} app_ui_application_t;

#define APP_UI_FILES_PAGE_SIZE 6U
#define APP_UI_LOGS_PAGE_SIZE  7U

typedef enum
{
    APP_UI_LOGS_ACTION_NONE = 0,
    APP_UI_LOGS_ACTION_CLEAR,
    APP_UI_LOGS_ACTION_EXPORT,
    APP_UI_LOGS_ACTION_PREVIOUS,
    APP_UI_LOGS_ACTION_NEXT
} app_ui_logs_action_t;

#define APP_UI_DRAW_CANVAS_LEFT    28U
#define APP_UI_DRAW_CANVAS_TOP     70U
#define APP_UI_DRAW_CANVAS_RIGHT   612U
#define APP_UI_DRAW_CANVAS_BOTTOM  422U

typedef enum
{
    APP_UI_DRAW_ACTION_NONE = 0,
    APP_UI_DRAW_ACTION_CLEAR,
    APP_UI_DRAW_ACTION_SAVE,
    APP_UI_DRAW_ACTION_OPEN,
    APP_UI_DRAW_ACTION_BLACK,
    APP_UI_DRAW_ACTION_RED,
    APP_UI_DRAW_ACTION_GREEN,
    APP_UI_DRAW_ACTION_BLUE
} app_ui_draw_action_t;

typedef enum
{
    APP_UI_FILES_ACTION_NONE = 0,
    APP_UI_FILES_ACTION_NEW,
    APP_UI_FILES_ACTION_OPEN,
    APP_UI_FILES_ACTION_EDIT,
    APP_UI_FILES_ACTION_SAVE,
    APP_UI_FILES_ACTION_DELETE,
    APP_UI_FILES_ACTION_PREVIOUS,
    APP_UI_FILES_ACTION_NEXT,
    APP_UI_FILES_ACTION_REFRESH,
    APP_UI_FILES_ACTION_LIST
} app_ui_files_action_t;

void app_ui_show_boot(uint8_t touch_available, const char *controller_id);
void app_ui_show_login(uint8_t touch_available);
void app_ui_update_login(uint8_t digit_count,
                         uint8_t failed_attempts,
                         const char *message,
                         uint8_t is_error);
int8_t app_ui_login_key_at(uint16_t x, uint16_t y);

void app_ui_show_locked(uint32_t remaining_seconds);
void app_ui_update_locked(uint32_t remaining_seconds);

void app_ui_show_desktop(uint8_t touch_available,
                         const char *controller_id,
                         uint32_t uptime_seconds);
void app_ui_update_desktop_time(uint32_t uptime_seconds);
void app_ui_move_cursor(uint16_t x, uint16_t y);
int8_t app_ui_desktop_icon_at(uint16_t x, uint16_t y);
void app_ui_select_desktop_icon(int8_t icon_index);

uint8_t app_ui_back_button_at(uint16_t x, uint16_t y);
void app_ui_show_application(app_ui_application_t application);
void app_ui_show_draw(uint16_t selected_color,
                      const char *status,
                      uint8_t busy);
void app_ui_update_draw_controls(uint16_t selected_color,
                                 const char *status,
                                 uint8_t busy);
void app_ui_draw_stroke(uint16_t x1,
                        uint16_t y1,
                        uint16_t x2,
                        uint16_t y2,
                        uint16_t color);
app_ui_draw_action_t app_ui_draw_action_at(uint16_t x, uint16_t y);
void app_ui_show_logs(const app_log_entry_t *entries,
                      uint8_t entry_count,
                      uint8_t page,
                      const char *status,
                      uint8_t busy,
                      uint8_t clear_armed);
app_ui_logs_action_t app_ui_logs_action_at(uint16_t x, uint16_t y);
void app_ui_show_files_list(const app_storage_file_t *files,
                            uint8_t file_count,
                            uint8_t page,
                            int8_t selected_file,
                            const char *status,
                            uint8_t busy);
void app_ui_show_file_content(const char *name,
                              const char *content,
                              uint32_t file_size,
                              uint8_t dirty,
                              const char *status,
                              uint8_t busy);
int8_t app_ui_files_row_at(uint16_t x, uint16_t y);
app_ui_files_action_t app_ui_files_action_at(uint16_t x,
                                             uint16_t y,
                                             uint8_t content_view);
void app_ui_show_monitor(const app_monitor_snapshot_t *snapshot);
void app_ui_update_monitor(const app_monitor_snapshot_t *snapshot);

#endif
