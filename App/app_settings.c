#include "app_settings.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "FreeRTOS.h"
#include "app_audio.h"
#include "app_input.h"
#include "app_logs.h"
#include "app_screen.h"
#include "app_storage.h"
#include "app_ui.h"
#include "task.h"

#define APP_SETTINGS_FILE_NAME       "SETTINGS.CFG"
#define APP_SETTINGS_FILE_VERSION      3U
#define APP_SETTINGS_LEGACY_VERSION    1U
#define APP_SETTINGS_PREVIOUS_VERSION  2U

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
    char status[64];
    app_settings_config_t config;
    app_settings_document_t io_document;
} app_settings_state_t;

static app_settings_state_t g_settings;

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

static void app_settings_set_status(const char *status)
{
    app_settings_copy_text(g_settings.status,
                           sizeof(g_settings.status), status);
}

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

static void app_settings_apply_config(const app_settings_config_t *config)
{
    app_input_set_cursor_sensitivity(config->cursor_sensitivity);
    app_ui_set_cursor_size(config->cursor_size);
    app_screen_set_brightness(config->brightness_percent);
    app_audio_set_volume(config->volume_percent);
}

static void app_settings_set_config(const app_settings_config_t *config)
{
    taskENTER_CRITICAL();
    g_settings.config = *config;
    taskEXIT_CRITICAL();
    app_settings_apply_config(config);
}

static app_settings_config_t app_settings_get_config(void)
{
    app_settings_config_t config;

    taskENTER_CRITICAL();
    config = g_settings.config;
    taskEXIT_CRITICAL();
    return config;
}

static uint8_t app_settings_timeout_is_valid(uint32_t timeout_seconds)
{
    return (timeout_seconds == APP_SETTINGS_SCREEN_OFF_DISABLED ||
            timeout_seconds == APP_SETTINGS_SCREEN_OFF_30_SECONDS ||
            timeout_seconds == APP_SETTINGS_SCREEN_OFF_60_SECONDS ||
            timeout_seconds == APP_SETTINGS_SCREEN_OFF_120_SECONDS) ? 1U : 0U;
}

static uint8_t app_settings_brightness_is_valid(uint8_t brightness_percent)
{
    return (brightness_percent == 25U || brightness_percent == 50U ||
            brightness_percent == 75U || brightness_percent == 100U) ? 1U : 0U;
}

static uint8_t app_settings_volume_is_valid(uint8_t volume_percent)
{
    return (volume_percent == 0U || volume_percent == 25U ||
            volume_percent == 50U || volume_percent == 75U ||
            volume_percent == 100U) ? 1U : 0U;
}

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

static void app_settings_redraw(void)
{
    app_settings_config_t config;

    if (!g_settings.active)
    {
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

static void app_settings_save(void)
{
    app_storage_request_t request;
    app_settings_config_t config;

    if (g_settings.busy)
    {
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
        app_settings_set_status("SETTINGS SAVE QUEUE IS FULL");
        app_logs_add(APP_LOG_LEVEL_WARNING, "SETTINGS", "SAVE QUEUE FULL");
        app_settings_redraw();
        return;
    }

    g_settings.busy = 1U;
    app_settings_set_status("SAVING SETTINGS.CFG");
    app_settings_redraw();
}

static void app_settings_change(const app_settings_config_t *config,
                                const char *status)
{
    app_settings_set_config(config);
    g_settings.dirty = 1U;
    app_settings_set_status(status);
    app_settings_save();
}

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

void app_settings_open(void)
{
    app_settings_init();
    g_settings.active = 1U;
    app_settings_redraw();
}

void app_settings_close(void)
{
    g_settings.active = 0U;
}

void app_settings_handle_event(const app_input_event_t *event)
{
    app_ui_settings_action_t action;
    app_settings_config_t config;

    if (!g_settings.active || event == NULL ||
        event->type != APP_INPUT_EVENT_DOWN || g_settings.busy)
    {
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
                app_settings_set_status("SETTINGS.CFG SAVE FAILED");
                app_logs_add(APP_LOG_LEVEL_ERROR, "SETTINGS", "CONFIG SAVE FAILED");
            }
        }
        app_settings_redraw();
    }
}

uint32_t app_settings_get_screen_timeout_seconds(void)
{
    return app_settings_get_config().idle_timeout_seconds;
}

uint8_t app_settings_get_serial_output_enabled(void)
{
    return app_settings_get_config().serial_output_enabled;
}

uint8_t app_settings_get_cursor_sensitivity(void)
{
    return app_settings_get_config().cursor_sensitivity;
}

uint8_t app_settings_get_cursor_size(void)
{
    return app_settings_get_config().cursor_size;
}

uint8_t app_settings_get_brightness_percent(void)
{
    return app_settings_get_config().brightness_percent;
}

uint8_t app_settings_get_volume_percent(void)
{
    return app_settings_get_config().volume_percent;
}
