#ifndef APP_LOGS_H
#define APP_LOGS_H

#include <stdint.h>

#include "app_input.h"

#define APP_LOG_MAX_ENTRIES       64U
#define APP_LOG_MODULE_LENGTH     9U
#define APP_LOG_MESSAGE_LENGTH    49U

typedef enum
{
    APP_LOG_LEVEL_INFO = 0,
    APP_LOG_LEVEL_WARNING,
    APP_LOG_LEVEL_ERROR
} app_log_level_t;

typedef struct
{
    uint32_t uptime_seconds;
    app_log_level_t level;
    char module[APP_LOG_MODULE_LENGTH];
    char message[APP_LOG_MESSAGE_LENGTH];
} app_log_entry_t;

void app_logs_init(void);
void app_logs_add(app_log_level_t level,
                  const char *module,
                  const char *message);
void app_logs_open(void);
void app_logs_close(void);
void app_logs_handle_event(const app_input_event_t *event);
void app_logs_update(void);

#endif
