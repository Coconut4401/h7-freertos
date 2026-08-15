#ifndef APP_RUNTIME_H
#define APP_RUNTIME_H

#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"

typedef enum
{
    APP_STATE_BOOT = 0,
    APP_STATE_LOGIN,
    APP_STATE_LOCKED,
    APP_STATE_DESKTOP,
    APP_STATE_APPLICATION,
    APP_STATE_SCREEN_OFF
} app_state_t;

typedef struct
{
    QueueHandle_t event_queue;
    uint8_t touch_available;
    const char *controller_id;
} app_runtime_context_t;

void AppRuntimeTask(void *argument);

#endif
