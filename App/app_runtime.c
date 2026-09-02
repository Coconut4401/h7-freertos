#include "app_runtime.h"

#include <stdint.h>
#include <stdio.h>

#include "app_draw.h"
#include "app_files.h"
#include "app_input.h"
#include "app_logs.h"
#include "app_music.h"
#include "app_monitor.h"
#include "app_rtc.h"
#include "app_screen.h"
#include "app_settings.h"
#include "app_health.h"
#include "app_fault.h"
#include "app_ui.h"
#include "task.h"

#define APP_BOOT_TIME_MS          2000U
#define APP_LOCK_TIME_MS          10000U
#define APP_MAX_LOGIN_FAILURES    3U
#define APP_PASSWORD_LENGTH       4U
#define APP_MONITOR_UPDATE_MS     500U

typedef struct
{
    app_state_t state;
    char entered_pin[APP_PASSWORD_LENGTH];
    uint8_t entered_length;
    uint8_t failed_attempts;
    uint16_t cursor_x;
    uint16_t cursor_y;
    TickType_t state_deadline;
    uint32_t last_display_second;
    uint32_t last_lock_second;
    TickType_t last_monitor_update;
    TickType_t last_activity;
    uint8_t suppress_input_until_release;
    uint8_t mouse_connection_known;
    uint8_t mouse_connected;
    int8_t selected_icon;
    int8_t active_application;
} app_runtime_state_t;

static const char g_password[APP_PASSWORD_LENGTH] = {'1', '2', '3', '4'};

static uint32_t app_runtime_clock_seconds(TickType_t now)
{
    if (app_rtc_is_available())
    {
        return app_rtc_get_seconds_of_day();
    }
    return (uint32_t)(now / configTICK_RATE_HZ);
}

static const char *app_runtime_application_name(int8_t application)
{
    switch (application)
    {
        case APP_UI_APP_FILES:
            return "FILES OPENED";

        case APP_UI_APP_DRAW:
            return "DRAW OPENED";

        case APP_UI_APP_MUSIC:
            return "MUSIC OPENED";

        case APP_UI_APP_LOGS:
            return "LOGS OPENED";

        case APP_UI_APP_MONITOR:
            return "MONITOR OPENED";

        case APP_UI_APP_SETTINGS:
            return "SETTINGS OPENED";

        default:
            return "UNKNOWN APP OPENED";
    }
}

static uint8_t app_runtime_deadline_reached(TickType_t now, TickType_t deadline)
{
    return ((int32_t)(now - deadline) >= 0) ? 1U : 0U;
}

static uint8_t app_runtime_password_matches(const app_runtime_state_t *runtime)
{
    uint8_t index;

    if (runtime->entered_length != APP_PASSWORD_LENGTH)
    {
        return 0U;
    }

    for (index = 0U; index < APP_PASSWORD_LENGTH; index++)
    {
        if (runtime->entered_pin[index] != g_password[index])
        {
            return 0U;
        }
    }

    return 1U;
}

static void app_runtime_enter_login(app_runtime_state_t *runtime,
                                    const app_runtime_context_t *context)
{
    runtime->state = APP_STATE_LOGIN;
    runtime->active_application = -1;
    runtime->entered_length = 0U;
    app_logs_add(APP_LOG_LEVEL_INFO, "AUTH", "LOGIN SCREEN READY");
    app_ui_show_login(context->touch_available);
    app_ui_update_login(0U, runtime->failed_attempts, "ENTER PIN", 0U);
}

static void app_runtime_enter_desktop(app_runtime_state_t *runtime,
                                      const app_runtime_context_t *context,
                                      TickType_t now)
{
    runtime->state = APP_STATE_DESKTOP;
    runtime->active_application = -1;
    runtime->entered_length = 0U;
    runtime->failed_attempts = 0U;
    runtime->last_display_second = app_runtime_clock_seconds(now);
    runtime->last_activity = now;
    app_ui_show_desktop(context->touch_available,
                        context->controller_id,
                        runtime->last_display_second);
    app_ui_update_desktop_mouse(runtime->mouse_connection_known,
                                runtime->mouse_connected);
    if (runtime->selected_icon >= 0)
    {
        app_ui_select_desktop_icon(runtime->selected_icon);
    }
    app_ui_move_cursor(runtime->cursor_x, runtime->cursor_y);
}

static void app_runtime_enter_application(app_runtime_state_t *runtime,
                                          int8_t application,
                                          TickType_t now)
{
    app_monitor_snapshot_t snapshot;

    runtime->state = APP_STATE_APPLICATION;
    runtime->active_application = application;
    runtime->last_monitor_update = now;
    app_logs_add(APP_LOG_LEVEL_INFO, "DESKTOP",
                 app_runtime_application_name(application));

    if (application == APP_UI_APP_FILES)
    {
        app_files_open();
    }
    else if (application == APP_UI_APP_DRAW)
    {
        app_draw_open();
    }
    else if (application == APP_UI_APP_MUSIC)
    {
        app_music_open();
    }
    else if (application == APP_UI_APP_LOGS)
    {
        app_logs_open();
    }
    else if (application == APP_UI_APP_MONITOR)
    {
        app_monitor_get_snapshot(&snapshot);
        app_ui_show_monitor(&snapshot);
    }
    else if (application == APP_UI_APP_SETTINGS)
    {
        app_settings_open();
    }
    else
    {
        app_ui_show_application((app_ui_application_t)application);
    }
    if (application != APP_UI_APP_DRAW)
    {
        app_ui_move_cursor(runtime->cursor_x, runtime->cursor_y);
    }
}

static void app_runtime_submit_pin(app_runtime_state_t *runtime,
                                   const app_runtime_context_t *context,
                                   TickType_t now)
{
    if (runtime->entered_length < APP_PASSWORD_LENGTH)
    {
        app_ui_update_login(runtime->entered_length,
                            runtime->failed_attempts,
                            "ENTER 4 DIGITS", 1U);
        return;
    }

    if (app_runtime_password_matches(runtime))
    {
        app_logs_add(APP_LOG_LEVEL_INFO, "AUTH", "LOGIN SUCCESS");
        app_runtime_enter_desktop(runtime, context, now);
        return;
    }

    runtime->entered_length = 0U;
    runtime->failed_attempts++;
    if (runtime->failed_attempts >= APP_MAX_LOGIN_FAILURES)
    {
        app_logs_add(APP_LOG_LEVEL_ERROR, "AUTH", "LOGIN LOCKED");
        runtime->state = APP_STATE_LOCKED;
        runtime->state_deadline = now + pdMS_TO_TICKS(APP_LOCK_TIME_MS);
        runtime->last_lock_second = APP_LOCK_TIME_MS / 1000U;
        app_ui_show_locked(runtime->last_lock_second);
    }
    else
    {
        app_logs_add(APP_LOG_LEVEL_WARNING, "AUTH", "WRONG PASSWORD");
        app_ui_update_login(0U, runtime->failed_attempts,
                            "WRONG PASSWORD", 1U);
    }
}

static void app_runtime_handle_login_event(app_runtime_state_t *runtime,
                                           const app_runtime_context_t *context,
                                           const app_input_event_t *event,
                                           TickType_t now)
{
    int8_t key;

    if (event->source == APP_INPUT_SOURCE_MOUSE &&
        (event->type == APP_INPUT_EVENT_DOWN ||
         event->type == APP_INPUT_EVENT_MOVE))
    {
        runtime->cursor_x = event->x;
        runtime->cursor_y = event->y;
        app_ui_move_cursor(runtime->cursor_x, runtime->cursor_y);
    }

    if (event->type != APP_INPUT_EVENT_DOWN)
    {
        return;
    }

    key = app_ui_login_key_at(event->x, event->y);
    if (key >= 0 && key <= 9)
    {
        if (runtime->entered_length < APP_PASSWORD_LENGTH)
        {
            runtime->entered_pin[runtime->entered_length] = (char)('0' + key);
            runtime->entered_length++;
        }
        app_ui_update_login(runtime->entered_length,
                            runtime->failed_attempts,
                            "ENTER PIN", 0U);
    }
    else if (key == APP_UI_KEY_CLEAR)
    {
        runtime->entered_length = 0U;
        app_ui_update_login(0U, runtime->failed_attempts, "CLEARED", 0U);
    }
    else if (key == APP_UI_KEY_ENTER)
    {
        app_runtime_submit_pin(runtime, context, now);
    }
}

static uint8_t app_runtime_handle_mouse_connection(
    app_runtime_state_t *runtime,
    const app_input_event_t *event)
{
    uint8_t was_known;
    uint8_t was_connected;

    if (event->type != APP_INPUT_EVENT_MOUSE_CONNECTED &&
        event->type != APP_INPUT_EVENT_MOUSE_DISCONNECTED)
    {
        return 0U;
    }

    was_known = runtime->mouse_connection_known;
    was_connected = runtime->mouse_connected;
    runtime->mouse_connection_known = 1U;
    runtime->mouse_connected =
        (event->type == APP_INPUT_EVENT_MOUSE_CONNECTED) ? 1U : 0U;

    if (runtime->mouse_connected)
    {
        app_logs_add(APP_LOG_LEVEL_INFO, "CH9350",
                     (was_known && !was_connected) ?
                         "MOUSE RECONNECTED" : "MOUSE CONNECTED");
    }
    else
    {
        app_logs_add(APP_LOG_LEVEL_WARNING, "CH9350",
                     (was_known && was_connected) ?
                         "MOUSE DISCONNECTED" : "MOUSE NOT CONNECTED");
    }

    if (runtime->state == APP_STATE_DESKTOP)
    {
        app_ui_update_desktop_mouse(runtime->mouse_connection_known,
                                    runtime->mouse_connected);
    }
    return 1U;
}

static uint8_t app_runtime_event_can_wake(const app_input_event_t *event)
{
    return (event->type == APP_INPUT_EVENT_DOWN ||
            event->type == APP_INPUT_EVENT_MOVE ||
            event->type == APP_INPUT_EVENT_SCROLL ||
            event->type == APP_INPUT_EVENT_BACK) ? 1U : 0U;
}

static void app_runtime_handle_desktop_event(app_runtime_state_t *runtime,
                                             const app_input_event_t *event,
                                             TickType_t now)
{
    int8_t icon;

    if (event->type == APP_INPUT_EVENT_DOWN &&
        app_ui_desktop_sleep_button_at(event->x, event->y))
    {
        runtime->suppress_input_until_release = 1U;
        app_screen_off();
        app_logs_add(APP_LOG_LEVEL_INFO, "POWER", "SCREEN OFF - MANUAL");
        return;
    }

    if (event->type == APP_INPUT_EVENT_DOWN ||
        event->type == APP_INPUT_EVENT_MOVE)
    {
        runtime->cursor_x = event->x;
        runtime->cursor_y = event->y;
        app_ui_move_cursor(runtime->cursor_x, runtime->cursor_y);
    }

    if (event->type == APP_INPUT_EVENT_DOWN)
    {
        icon = app_ui_desktop_icon_at(event->x, event->y);
        if (icon >= 0)
        {
            if (icon == runtime->selected_icon)
            {
                app_runtime_enter_application(runtime, icon, now);
            }
            else
            {
                runtime->selected_icon = icon;
                app_ui_select_desktop_icon(icon);
            }
        }
    }
}

static void app_runtime_handle_application_event(app_runtime_state_t *runtime,
                                                 const app_runtime_context_t *context,
                                                 const app_input_event_t *event,
                                                 TickType_t now)
{
    if (event->type == APP_INPUT_EVENT_DOWN &&
        app_ui_application_sleep_button_at(event->x, event->y))
    {
        runtime->suppress_input_until_release = 1U;
        app_screen_off();
        app_logs_add(APP_LOG_LEVEL_INFO, "POWER", "SCREEN OFF - MANUAL");
        return;
    }

    if ((runtime->active_application != APP_UI_APP_DRAW ||
         event->source == APP_INPUT_SOURCE_MOUSE) &&
        (event->type == APP_INPUT_EVENT_DOWN ||
         event->type == APP_INPUT_EVENT_MOVE))
    {
        runtime->cursor_x = event->x;
        runtime->cursor_y = event->y;
        app_ui_move_cursor(runtime->cursor_x, runtime->cursor_y);
    }

    if (event->type == APP_INPUT_EVENT_BACK ||
        (event->type == APP_INPUT_EVENT_DOWN &&
         app_ui_back_button_at(event->x, event->y)))
    {
        if (runtime->active_application == APP_UI_APP_FILES)
        {
            app_files_close();
        }
        else if (runtime->active_application == APP_UI_APP_DRAW)
        {
            app_draw_close();
        }
        else if (runtime->active_application == APP_UI_APP_MUSIC)
        {
            app_music_close();
        }
        else if (runtime->active_application == APP_UI_APP_LOGS)
        {
            app_logs_close();
        }
        else if (runtime->active_application == APP_UI_APP_SETTINGS)
        {
            app_settings_close();
        }
        app_logs_add(APP_LOG_LEVEL_INFO, "DESKTOP", "APPLICATION CLOSED");
        runtime->selected_icon = runtime->active_application;
        app_runtime_enter_desktop(runtime, context, now);
        return;
    }

    if (runtime->active_application == APP_UI_APP_FILES)
    {
        app_files_handle_event(event);
    }
    else if (runtime->active_application == APP_UI_APP_DRAW)
    {
        app_draw_handle_event(event);
    }
    else if (runtime->active_application == APP_UI_APP_MUSIC)
    {
        app_music_handle_event(event);
    }
    else if (runtime->active_application == APP_UI_APP_LOGS)
    {
        app_logs_handle_event(event);
    }
    else if (runtime->active_application == APP_UI_APP_SETTINGS)
    {
        app_settings_handle_event(event);
    }
}

static void app_runtime_update(app_runtime_state_t *runtime,
                               const app_runtime_context_t *context,
                               TickType_t now)
{
    uint32_t current_second;
    uint32_t remaining_ticks;
    uint32_t remaining_seconds;
    uint32_t screen_timeout_seconds;
    app_monitor_snapshot_t snapshot;

    app_files_update();
    app_draw_update();
    app_music_update();
    app_logs_update();
    app_settings_update();
    app_rtc_update();

    screen_timeout_seconds = app_settings_get_screen_timeout_seconds();
    if (!app_screen_is_off() &&
        (runtime->state == APP_STATE_LOGIN ||
         runtime->state == APP_STATE_DESKTOP ||
         runtime->state == APP_STATE_APPLICATION) &&
        screen_timeout_seconds != APP_SETTINGS_SCREEN_OFF_DISABLED &&
        (TickType_t)(now - runtime->last_activity) >=
        pdMS_TO_TICKS(screen_timeout_seconds * 1000U))
    {
        app_screen_off();
        app_logs_add(APP_LOG_LEVEL_INFO, "POWER", "SCREEN OFF - TIMEOUT");
        return;
    }

    if (runtime->state == APP_STATE_BOOT &&
        app_runtime_deadline_reached(now, runtime->state_deadline))
    {
        app_runtime_enter_login(runtime, context);
    }
    else if (runtime->state == APP_STATE_LOCKED)
    {
        if (app_runtime_deadline_reached(now, runtime->state_deadline))
        {
            runtime->failed_attempts = 0U;
            app_runtime_enter_login(runtime, context);
        }
        else
        {
            remaining_ticks = (uint32_t)(runtime->state_deadline - now);
            remaining_seconds = (remaining_ticks + configTICK_RATE_HZ - 1U) /
                                configTICK_RATE_HZ;
            if (remaining_seconds != runtime->last_lock_second)
            {
                runtime->last_lock_second = remaining_seconds;
                app_ui_update_locked(remaining_seconds);
            }
        }
    }
    else if (runtime->state == APP_STATE_DESKTOP)
    {
        current_second = app_runtime_clock_seconds(now);
        if (current_second != runtime->last_display_second)
        {
            runtime->last_display_second = current_second;
            app_ui_update_desktop_time(current_second);
        }
    }
    else if (runtime->state == APP_STATE_APPLICATION &&
             runtime->active_application == APP_UI_APP_MONITOR &&
             (TickType_t)(now - runtime->last_monitor_update) >=
             pdMS_TO_TICKS(APP_MONITOR_UPDATE_MS))
    {
        runtime->last_monitor_update = now;
        app_monitor_get_snapshot(&snapshot);
        app_ui_update_monitor(&snapshot);
    }
}

void AppRuntimeTask(void *argument)
{
    const app_runtime_context_t *context;
    app_runtime_state_t runtime;
    app_input_event_t event;
    TickType_t now;

    context = (const app_runtime_context_t *)argument;
    runtime.state = APP_STATE_BOOT;
    runtime.entered_length = 0U;
    runtime.failed_attempts = 0U;
    runtime.cursor_x = APP_LCD_WIDTH / 2U;
    runtime.cursor_y = APP_LCD_HEIGHT / 2U;
    runtime.last_display_second = 0U;
    runtime.last_lock_second = 0U;
    runtime.last_monitor_update = 0U;
    runtime.last_activity = 0U;
    runtime.suppress_input_until_release = 0U;
    runtime.mouse_connection_known = 0U;
    runtime.mouse_connected = 0U;
    runtime.selected_icon = -1;
    runtime.active_application = -1;

    app_logs_init();
    {
        app_fault_record_t fault;
        if (app_fault_get_pending(&fault))
        {
            char message[APP_LOG_MESSAGE_LENGTH];
            snprintf(message, sizeof(message), "RESET TYPE %lu TASK %.15s",
                     (unsigned long)fault.type, fault.task);
            app_logs_add(APP_LOG_LEVEL_ERROR, "FAULT", message);
        }
    }
    app_screen_init();
    app_rtc_init();
    app_logs_add(APP_LOG_LEVEL_INFO, "RTC",
                 app_rtc_is_available() ? "DS3231 TIME ONLINE" :
                 "DS3231 NOT FOUND - UPTIME FALLBACK");
    app_settings_init();
    app_logs_add(APP_LOG_LEVEL_INFO, "SYSTEM", "RUNTIME TASK STARTED");

    now = xTaskGetTickCount();
    runtime.last_activity = now;
    runtime.state_deadline = now + pdMS_TO_TICKS(APP_BOOT_TIME_MS);
    app_ui_show_boot(context->touch_available, context->controller_id);

    while (1)
    {
        app_health_beat(APP_HEALTH_RUNTIME);
        if (xQueueReceive(context->event_queue, &event,
                          pdMS_TO_TICKS(50U)) == pdPASS)
        {
            now = xTaskGetTickCount();
            if (app_runtime_handle_mouse_connection(&runtime, &event))
            {
                /* Link-state notifications do not count as user activity. */
            }
            else if (app_screen_is_off())
            {
                if (runtime.suppress_input_until_release)
                {
                    if (event.type == APP_INPUT_EVENT_UP)
                    {
                        runtime.suppress_input_until_release = 0U;
                    }
                }
                else if (app_runtime_event_can_wake(&event))
                {
                    app_screen_on();
                    runtime.last_activity = now;
                    runtime.suppress_input_until_release =
                        (event.type == APP_INPUT_EVENT_DOWN) ? 1U : 0U;
                    app_logs_add(APP_LOG_LEVEL_INFO, "POWER",
                                 "SCREEN WAKE BY INPUT");
                }
            }
            else if (runtime.suppress_input_until_release)
            {
                runtime.last_activity = now;
                if (event.type == APP_INPUT_EVENT_UP)
                {
                    runtime.suppress_input_until_release = 0U;
                }
            }
            else
            {
                runtime.last_activity = now;
                if (runtime.state == APP_STATE_LOGIN)
                {
                    app_runtime_handle_login_event(&runtime, context,
                                                   &event, now);
                }
                else if (runtime.state == APP_STATE_DESKTOP)
                {
                    app_runtime_handle_desktop_event(&runtime, &event, now);
                }
                else if (runtime.state == APP_STATE_APPLICATION)
                {
                    app_runtime_handle_application_event(&runtime, context,
                                                         &event, now);
                }
            }
        }

        now = xTaskGetTickCount();
        app_runtime_update(&runtime, context, now);
        app_fault_publish_pending();
    }
}
