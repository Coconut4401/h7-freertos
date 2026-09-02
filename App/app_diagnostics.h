#ifndef APP_DIAGNOSTICS_H
#define APP_DIAGNOSTICS_H

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"

/*
 * Production value is 0. Use exactly one non-zero mode in a dedicated
 * diagnostic build, then restore 0 before producing the release HEX.
 * 1: stack overflow, 2: heap exhaustion, 3: InputTask heartbeat timeout,
 * 4: input queue saturation and recovery.
 */
#define APP_DIAGNOSTIC_MODE 0U

typedef struct
{
    QueueHandle_t input_queue;
    TaskHandle_t input_task;
} app_diagnostics_context_t;

void AppDiagnosticsTask(void *argument);

#endif
