#include "app_diagnostics.h"

#include <stdint.h>

#include "app_input.h"
#include "app_logs.h"

#if APP_DIAGNOSTIC_MODE == 1U
__attribute__((noinline)) static void app_diagnostics_overflow(uint32_t depth)
{
    volatile uint8_t padding[256];
    uint32_t index;

    for (index = 0U; index < sizeof(padding); index++)
    {
        padding[index] = (uint8_t)(depth + index);
    }
    app_diagnostics_overflow(depth + padding[depth & 0xFFU]);
}
#endif

void AppDiagnosticsTask(void *argument)
{
    const app_diagnostics_context_t *context;

    context = (const app_diagnostics_context_t *)argument;
    vTaskDelay(pdMS_TO_TICKS(5000U));

#if APP_DIAGNOSTIC_MODE == 1U
    app_logs_add(APP_LOG_LEVEL_WARNING, "DIAG", "TRIGGER STACK OVERFLOW");
    app_diagnostics_overflow(1U);
#elif APP_DIAGNOSTIC_MODE == 2U
    app_logs_add(APP_LOG_LEVEL_WARNING, "DIAG", "TRIGGER HEAP EXHAUSTION");
    while (pvPortMalloc(4096U) != NULL)
    {
    }
#elif APP_DIAGNOSTIC_MODE == 3U
    app_logs_add(APP_LOG_LEVEL_WARNING, "DIAG", "SUSPEND INPUT TASK");
    vTaskSuspend(context->input_task);
#elif APP_DIAGNOSTIC_MODE == 4U
    {
        app_input_event_t event;

        event.type = APP_INPUT_EVENT_MOVE;
        event.source = APP_INPUT_SOURCE_TOUCH;
        event.x = APP_LCD_WIDTH / 2U;
        event.y = APP_LCD_HEIGHT / 2U;
        event.wheel = 0;
        event.buttons = 0U;
        event.tick = (uint32_t)xTaskGetTickCount();
        while (xQueueSend(context->input_queue, &event, 0U) == pdPASS)
        {
        }
        event.type = APP_INPUT_EVENT_BACK;
        (void)app_input_post_event(context->input_queue, &event);
        vTaskDelay(pdMS_TO_TICKS(1000U));
        (void)app_input_post_event(context->input_queue, &event);
        app_logs_add(APP_LOG_LEVEL_WARNING, "DIAG",
                     "INPUT QUEUE RECOVERY TESTED");
    }
#else
    (void)context;
#endif

    vTaskDelete(NULL);
}
