#ifndef CH9350_H
#define CH9350_H

#include <stdint.h>

typedef struct
{
    uint8_t buttons;
    int8_t delta_x;
    int8_t delta_y;
    int8_t wheel;
} ch9350_mouse_report_t;

typedef enum
{
    CH9350_EVENT_MOUSE_REPORT = 0,
    CH9350_EVENT_CONNECTION
} ch9350_event_type_t;

typedef struct
{
    ch9350_event_type_t type;
    ch9350_mouse_report_t mouse_report;
    uint8_t connection_changed;
    uint8_t mouse_connected;
} ch9350_event_t;

typedef struct
{
    uint32_t mouse_report_count;
    uint32_t state_frame_count;
    uint32_t connect_event_count;
    uint32_t disconnect_event_count;
    uint32_t discarded_frame_count;
    uint32_t sync_error_count;
    uint32_t uart_dropped_count;
    uint8_t connection_known;
    uint8_t mouse_connected;
    uint8_t last_state_value;
} ch9350_stats_t;

void ch9350_init(void);
uint8_t ch9350_read_event(ch9350_event_t *event);
void ch9350_get_stats(ch9350_stats_t *stats);

#endif
