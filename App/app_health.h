#ifndef APP_HEALTH_H
#define APP_HEALTH_H

#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"

typedef enum
{
    APP_HEALTH_INPUT = 0U,
    APP_HEALTH_RUNTIME,
    APP_HEALTH_STORAGE,
    APP_HEALTH_AUDIO,
    APP_HEALTH_MONITOR,
    APP_HEALTH_COUNT
} app_health_task_id_t;

typedef struct
{
    TaskHandle_t input_task;
    TaskHandle_t runtime_task;
    TaskHandle_t storage_task;
    TaskHandle_t audio_task;
    TaskHandle_t monitor_task;
} app_health_context_t;

typedef struct
{
    uint32_t beat_mask;
    uint32_t healthy_mask;
    uint32_t timeout_mask;
    uint32_t low_stack_mask;
    uint32_t stack_words[APP_HEALTH_COUNT];
    uint32_t health_stack_words;
    uint32_t timer_stack_words;
    uint32_t current_heap;
    uint32_t minimum_heap;
    uint8_t watchdog_enabled;
    uint8_t startup_complete;
} app_health_snapshot_t;

void app_health_init(void);
void app_health_beat(app_health_task_id_t task_id);
void app_health_get_snapshot(app_health_snapshot_t *snapshot);
void AppHealthTask(void *argument);

#endif
