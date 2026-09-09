/**
 * @file app_ui.h
 * @brief 提供各应用共用的 LCD 绘制、窗口布局和界面更新接口。
 * @details 这是 app_ui 模块的接口文件（App/app_ui.h）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#ifndef APP_UI_H
/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define APP_UI_H

#include <stdint.h>

#include "app_logs.h"
#include "app_monitor.h"
#include "app_rtc.h"
#include "app_storage.h"
#include "app_audio.h"

#define APP_UI_KEY_NONE    (-1)
#define APP_UI_KEY_CLEAR   10
#define APP_UI_KEY_ENTER   11

typedef enum
{
    APP_UI_KEYBOARD_MODE_LOWER = 0,
    APP_UI_KEYBOARD_MODE_UPPER,
    APP_UI_KEYBOARD_MODE_NUMBER
} app_ui_keyboard_mode_t;

typedef enum
{
    APP_UI_KEYBOARD_ACTION_NONE = 0,
    APP_UI_KEYBOARD_ACTION_CHAR,
    APP_UI_KEYBOARD_ACTION_BACKSPACE,
    APP_UI_KEYBOARD_ACTION_SPACE,
    APP_UI_KEYBOARD_ACTION_ENTER,
    APP_UI_KEYBOARD_ACTION_SHIFT,
    APP_UI_KEYBOARD_ACTION_NUMBER,
    APP_UI_KEYBOARD_ACTION_OK,
    APP_UI_KEYBOARD_ACTION_CANCEL
} app_ui_keyboard_action_t;

typedef struct
{
    app_ui_keyboard_action_t action;
    /** Printable ASCII value when action is APP_UI_KEYBOARD_ACTION_CHAR. */
    char character;
} app_ui_keyboard_hit_t;

/** Hit-test the shared 800x480 keyboard layout; gaps return ACTION_NONE. */
app_ui_keyboard_hit_t app_ui_keyboard_hit_test(app_ui_keyboard_mode_t mode,
                                               uint16_t x,
                                               uint16_t y);
/** Draw the shared keyboard without changing application editing state. */
void app_ui_draw_keyboard(app_ui_keyboard_mode_t mode);

#define APP_UI_CURSOR_SIZE_SMALL     1U
#define APP_UI_CURSOR_SIZE_MEDIUM    2U
#define APP_UI_CURSOR_SIZE_LARGE     3U

/** @brief 模块数据类型：描述本模块维护的状态、配置或数据快照。 */
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

typedef enum
{
    APP_UI_SETTINGS_ACTION_NONE = 0,
    APP_UI_SETTINGS_ACTION_SENSITIVITY_LOW,
    APP_UI_SETTINGS_ACTION_SENSITIVITY_NORMAL,
    APP_UI_SETTINGS_ACTION_SENSITIVITY_HIGH,
    APP_UI_SETTINGS_ACTION_CURSOR_SMALL,
    APP_UI_SETTINGS_ACTION_CURSOR_MEDIUM,
    APP_UI_SETTINGS_ACTION_CURSOR_LARGE,
    APP_UI_SETTINGS_ACTION_BRIGHTNESS_25,
    APP_UI_SETTINGS_ACTION_BRIGHTNESS_50,
    APP_UI_SETTINGS_ACTION_BRIGHTNESS_75,
    APP_UI_SETTINGS_ACTION_BRIGHTNESS_100,
    APP_UI_SETTINGS_ACTION_VOLUME_0,
    APP_UI_SETTINGS_ACTION_VOLUME_25,
    APP_UI_SETTINGS_ACTION_VOLUME_50,
    APP_UI_SETTINGS_ACTION_VOLUME_75,
    APP_UI_SETTINGS_ACTION_VOLUME_100,
    APP_UI_SETTINGS_ACTION_SCREEN_OFF_DISABLED,
    APP_UI_SETTINGS_ACTION_SCREEN_OFF_30,
    APP_UI_SETTINGS_ACTION_SCREEN_OFF_60,
    APP_UI_SETTINGS_ACTION_SCREEN_OFF_120,
    APP_UI_SETTINGS_ACTION_SERIAL_ON,
    APP_UI_SETTINGS_ACTION_SERIAL_OFF,
    APP_UI_SETTINGS_ACTION_TIME,
    APP_UI_SETTINGS_ACTION_DEFAULTS,
    APP_UI_SETTINGS_ACTION_SAVE
} app_ui_settings_action_t;

typedef enum
{
    APP_UI_TIME_ACTION_NONE = 0,
    APP_UI_TIME_ACTION_YEAR_DOWN,
    APP_UI_TIME_ACTION_YEAR_UP,
    APP_UI_TIME_ACTION_MONTH_DOWN,
    APP_UI_TIME_ACTION_MONTH_UP,
    APP_UI_TIME_ACTION_DATE_DOWN,
    APP_UI_TIME_ACTION_DATE_UP,
    APP_UI_TIME_ACTION_HOUR_DOWN,
    APP_UI_TIME_ACTION_HOUR_UP,
    APP_UI_TIME_ACTION_MINUTE_DOWN,
    APP_UI_TIME_ACTION_MINUTE_UP,
    APP_UI_TIME_ACTION_SECOND_DOWN,
    APP_UI_TIME_ACTION_SECOND_UP,
    APP_UI_TIME_ACTION_CANCEL,
    APP_UI_TIME_ACTION_APPLY
} app_ui_time_action_t;

typedef enum
{
    APP_UI_MUSIC_ACTION_NONE = 0,
    APP_UI_MUSIC_ACTION_PREVIOUS,
    APP_UI_MUSIC_ACTION_PLAY_PAUSE,
    APP_UI_MUSIC_ACTION_STOP,
    APP_UI_MUSIC_ACTION_NEXT,
    APP_UI_MUSIC_ACTION_TEST_TONE,
    APP_UI_MUSIC_ACTION_RESCAN,
    APP_UI_MUSIC_ACTION_SEEK_BACK,
    APP_UI_MUSIC_ACTION_SEEK_FORWARD,
    APP_UI_MUSIC_ACTION_VOLUME_DOWN,
    APP_UI_MUSIC_ACTION_VOLUME_UP
} app_ui_music_action_t;

#define APP_UI_DRAW_CANVAS_LEFT    28U
#define APP_UI_DRAW_CANVAS_TOP     70U
#define APP_UI_DRAW_CANVAS_RIGHT   612U
#define APP_UI_DRAW_CANVAS_BOTTOM  422U

typedef enum
{
    APP_UI_DRAW_ACTION_NONE = 0,
    APP_UI_DRAW_ACTION_UNDO,
    APP_UI_DRAW_ACTION_SAVE,
    APP_UI_DRAW_ACTION_OPEN,
    APP_UI_DRAW_ACTION_BLACK,
    APP_UI_DRAW_ACTION_RED,
    APP_UI_DRAW_ACTION_GREEN,
    APP_UI_DRAW_ACTION_BLUE
} app_ui_draw_action_t;

typedef enum
{
    APP_UI_DRAW_SAVE_NONE = 0,
    APP_UI_DRAW_SAVE_NEW,
    APP_UI_DRAW_SAVE_RENAME,
    APP_UI_DRAW_SAVE_CANCEL,
    APP_UI_DRAW_SAVE_SAMPLE0,
    APP_UI_DRAW_SAVE_SAMPLE1,
    APP_UI_DRAW_SAVE_SAMPLE2,
    APP_UI_DRAW_SAVE_SAMPLE3,
    APP_UI_DRAW_SAVE_SAMPLE4,
    APP_UI_DRAW_SAVE_SAMPLE5,
    APP_UI_DRAW_SAVE_SAMPLE6,
    APP_UI_DRAW_SAVE_SAMPLE7
} app_ui_draw_save_action_t;

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
    APP_UI_FILES_ACTION_LIST,
    APP_UI_FILES_ACTION_RENAME,
    APP_UI_FILES_ACTION_DISCARD
} app_ui_files_action_t;

typedef enum
{
    APP_UI_FILE_CONFIRM_NONE = 0,
    APP_UI_FILE_CONFIRM_SAVE,
    APP_UI_FILE_CONFIRM_DISCARD,
    APP_UI_FILE_CONFIRM_CANCEL
} app_ui_file_confirm_action_t;

typedef enum
{
    APP_UI_MONITOR_ACTION_NONE = 0,
    APP_UI_MONITOR_ACTION_CAPTURE,
    APP_UI_MONITOR_ACTION_FLOOD,
    APP_UI_MONITOR_ACTION_RESET,
    APP_UI_MONITOR_ACTION_PAGE
} app_ui_monitor_action_t;

/**
 * @brief app_ui_show_boot：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param touch_available 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param controller_id 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_ui_show_boot(uint8_t touch_available, const char *controller_id);
/**
 * @brief app_ui_show_login：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param touch_available 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_ui_show_login(uint8_t touch_available);
/**
 * @brief app_ui_update_login：使用最新数据更新缓存、硬件输出或界面显示状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param digit_count 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param failed_attempts 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param message 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param is_error 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_ui_update_login(uint8_t digit_count,
                         uint8_t failed_attempts,
                         const char *message,
                         uint8_t is_error);
/**
 * @brief app_ui_login_key_at：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
int8_t app_ui_login_key_at(uint16_t x, uint16_t y);

/**
 * @brief app_ui_show_locked：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param remaining_seconds 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_ui_show_locked(uint32_t remaining_seconds);
/**
 * @brief app_ui_update_locked：使用最新数据更新缓存、硬件输出或界面显示状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param remaining_seconds 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_ui_update_locked(uint32_t remaining_seconds);

/**
 * @brief app_ui_show_desktop：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param touch_available 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param controller_id 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param uptime_seconds 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_ui_show_desktop(uint8_t touch_available,
                         const char *controller_id,
                         uint32_t uptime_seconds);
/**
 * @brief app_ui_update_desktop_time：使用最新数据更新缓存、硬件输出或界面显示状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param uptime_seconds 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_ui_update_desktop_time(uint32_t uptime_seconds);
/**
 * @brief app_ui_update_desktop_mouse：使用最新数据更新缓存、硬件输出或界面显示状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param connection_known 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param mouse_connected 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_ui_update_desktop_mouse(uint8_t connection_known,
                                 uint8_t mouse_connected);
/**
 * @brief app_ui_move_cursor：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_ui_move_cursor(uint16_t x, uint16_t y);
/**
 * @brief app_ui_set_cursor_size：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param cursor_size 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_ui_set_cursor_size(uint8_t cursor_size);
/**
 * @brief app_ui_desktop_icon_at：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
int8_t app_ui_desktop_icon_at(uint16_t x, uint16_t y);
/**
 * @brief app_ui_select_desktop_icon：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param icon_index 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_ui_select_desktop_icon(int8_t icon_index);
/**
 * @brief app_ui_desktop_sleep_button_at：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t app_ui_desktop_sleep_button_at(uint16_t x, uint16_t y);

/**
 * @brief app_ui_back_button_at：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t app_ui_back_button_at(uint16_t x, uint16_t y);
/**
 * @brief app_ui_application_sleep_button_at：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t app_ui_application_sleep_button_at(uint16_t x, uint16_t y);
/**
 * @brief app_ui_show_application：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param application 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_ui_show_application(app_ui_application_t application);
/**
 * @brief app_ui_show_music：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param snapshot 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_ui_show_music(const app_audio_snapshot_t *snapshot);
/**
 * @brief app_ui_update_music：使用最新数据更新缓存、硬件输出或界面显示状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param snapshot 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param previous 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_ui_update_music(const app_audio_snapshot_t *snapshot,
                         const app_audio_snapshot_t *previous);
/**
 * @brief app_ui_music_action_at：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
app_ui_music_action_t app_ui_music_action_at(uint16_t x, uint16_t y);
/**
 * @brief app_ui_show_draw：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param selected_color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param status 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param busy 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_ui_show_draw(uint16_t selected_color,
                      const char *status,
                      uint8_t busy);
/**
 * @brief app_ui_update_draw_controls：使用最新数据更新缓存、硬件输出或界面显示状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param selected_color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param status 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param busy 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_ui_update_draw_controls(uint16_t selected_color,
                                 const char *status,
                                 uint8_t busy);
/**
 * @brief app_ui_draw_stroke：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x1 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y1 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param x2 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y2 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param color 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_ui_draw_stroke(uint16_t x1,
                        uint16_t y1,
                        uint16_t x2,
                        uint16_t y2,
                        uint16_t color);
/**
 * @brief app_ui_draw_action_at：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
app_ui_draw_action_t app_ui_draw_action_at(uint16_t x, uint16_t y);
void app_ui_show_draw_save_dialog(const char names[][13], uint8_t count,
                                  int8_t selected, const char *status);
app_ui_draw_save_action_t app_ui_draw_save_action_at(uint16_t x, uint16_t y);
/**
 * @brief app_ui_show_logs：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param entries 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param entry_count 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param page 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param status 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param busy 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param clear_armed 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_ui_show_logs(const app_log_entry_t *entries,
                      uint8_t entry_count,
                      uint8_t page,
                      const char *status,
                      uint8_t busy,
                      uint8_t clear_armed);
/**
 * @brief app_ui_logs_action_at：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
app_ui_logs_action_t app_ui_logs_action_at(uint16_t x, uint16_t y);
/**
 * @brief app_ui_show_settings：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param cursor_sensitivity 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param cursor_size 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param brightness_percent 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param volume_percent 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param idle_timeout_seconds 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param serial_output_enabled 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param dirty 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param status 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param busy 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_ui_show_settings(uint8_t cursor_sensitivity,
                           uint8_t cursor_size,
                           uint8_t brightness_percent,
                           uint8_t volume_percent,
                           uint32_t idle_timeout_seconds,
                          uint8_t serial_output_enabled,
                          uint8_t dirty,
                          const char *status,
                          uint8_t busy);
/**
 * @brief app_ui_settings_action_at：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
app_ui_settings_action_t app_ui_settings_action_at(uint16_t x, uint16_t y);
/**
 * @brief app_ui_show_time_settings：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param datetime 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param rtc_available 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_ui_show_time_settings(const app_rtc_datetime_t *datetime,
                               uint8_t rtc_available);
/**
 * @brief app_ui_time_action_at：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
app_ui_time_action_t app_ui_time_action_at(uint16_t x, uint16_t y);
/**
 * @brief app_ui_show_files_list：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param files 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param file_count 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param page 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param selected_file 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param status 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param busy 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_ui_show_files_list(const app_storage_file_t *files,
                            uint8_t file_count,
                            uint8_t page,
                            int8_t selected_file,
                            const char *status,
                            uint8_t busy);
/**
 * @brief app_ui_show_file_content：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param name 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param content 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param file_size 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param dirty 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param status 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param busy 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_ui_show_file_content(const char *name,
                              const char *content,
                              uint32_t file_size,
                              uint8_t dirty,
                              const char *status,
                              uint8_t busy);
void app_ui_show_file_editor(const char *title,
                             const char *name,
                             const char *content,
                             uint16_t content_length,
                             uint8_t name_editor,
                             const char *status,
                             app_ui_keyboard_mode_t keyboard_mode);
void app_ui_update_file_editor(const char *title,
                               const char *name,
                               const char *content,
                               uint16_t content_length,
                               uint8_t name_editor,
                               const char *status,
                               app_ui_keyboard_mode_t keyboard_mode);
void app_ui_show_file_confirmation(const char *name,
                                   const char *message);
app_ui_file_confirm_action_t app_ui_file_confirmation_action_at(uint16_t x,
                                                                uint16_t y);
/**
 * @brief app_ui_files_row_at：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
int8_t app_ui_files_row_at(uint16_t x, uint16_t y);
/**
 * @brief app_ui_files_action_at：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param x 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param y 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param content_view 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
app_ui_files_action_t app_ui_files_action_at(uint16_t x,
                                             uint16_t y,
                                             uint8_t content_view);
/**
 * @brief app_ui_show_monitor：根据输入参数和当前状态绘制或更新对应显示内容。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param snapshot 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_ui_show_monitor(const app_monitor_snapshot_t *snapshot);
/**
 * @brief app_ui_update_monitor：使用最新数据更新缓存、硬件输出或界面显示状态。
 * @details 此处为接口声明；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param snapshot 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_ui_update_monitor(const app_monitor_snapshot_t *snapshot);
/** @brief 返回 MONITOR 页底部输入测试按钮的命中结果。 */
app_ui_monitor_action_t app_ui_monitor_action_at(uint16_t x, uint16_t y);
/** @brief 切换 MONITOR 的系统页/输入调度页并重绘。 */
void app_ui_monitor_set_input_page(uint8_t input_page,
                                   const app_monitor_snapshot_t *snapshot);
uint8_t app_ui_monitor_is_input_page(void);

#endif
