#ifndef APP_DRAW_H
#define APP_DRAW_H

#include "app_input.h"

void app_draw_open(void);
void app_draw_close(void);
void app_draw_handle_event(const app_input_event_t *event);
void app_draw_update(void);

#endif
