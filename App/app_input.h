#ifndef APP_INPUT_H
#define APP_INPUT_H

#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"

#define APP_LCD_WIDTH            800U
#define APP_LCD_HEIGHT           480U
#define APP_INPUT_QUEUE_LENGTH   32U

#define APP_INPUT_SENSITIVITY_LOW       1U
#define APP_INPUT_SENSITIVITY_NORMAL    2U
#define APP_INPUT_SENSITIVITY_HIGH      3U

typedef enum
{
    APP_INPUT_EVENT_DOWN = 0,
    APP_INPUT_EVENT_MOVE,
    APP_INPUT_EVENT_UP,
    APP_INPUT_EVENT_SCROLL,
    APP_INPUT_EVENT_BACK,
    APP_INPUT_EVENT_MOUSE_CONNECTED,
    APP_INPUT_EVENT_MOUSE_DISCONNECTED
} app_input_event_type_t;

typedef enum
{
    APP_INPUT_SOURCE_TOUCH = 0,
    APP_INPUT_SOURCE_MOUSE
} app_input_source_t;

typedef struct
{
    app_input_event_type_t type;
    app_input_source_t source;
    uint16_t x;
    uint16_t y;
    int8_t wheel;
    uint8_t buttons;
    uint32_t tick;
} app_input_event_t;

typedef struct
{
    QueueHandle_t event_queue;
    uint8_t touch_available;
} app_input_task_context_t;

typedef struct
{
    uint32_t sent_count;
    uint32_t dropped_count;
    uint32_t throttled_move_count;
    uint32_t critical_drop_count;
} app_input_stats_t;

void AppInputTask(void *argument);
void app_input_get_stats(app_input_stats_t *stats);
BaseType_t app_input_post_event(QueueHandle_t queue,
                                const app_input_event_t *event);
void app_input_set_cursor_sensitivity(uint8_t sensitivity);
uint8_t app_input_get_cursor_sensitivity(void);

#endif
