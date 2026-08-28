#ifndef APP_SETTINGS_H
#define APP_SETTINGS_H

#include <stdint.h>

#include "app_input.h"

#define APP_SETTINGS_SCREEN_OFF_DISABLED  0U
#define APP_SETTINGS_SCREEN_OFF_30_SECONDS   30U
#define APP_SETTINGS_SCREEN_OFF_60_SECONDS   60U
#define APP_SETTINGS_SCREEN_OFF_120_SECONDS  120U

void app_settings_init(void);
void app_settings_open(void);
void app_settings_close(void);
void app_settings_handle_event(const app_input_event_t *event);
void app_settings_update(void);
uint32_t app_settings_get_screen_timeout_seconds(void);
uint8_t app_settings_get_serial_output_enabled(void);
uint8_t app_settings_get_cursor_sensitivity(void);
uint8_t app_settings_get_cursor_size(void);
uint8_t app_settings_get_brightness_percent(void);
uint8_t app_settings_get_volume_percent(void);

#endif
