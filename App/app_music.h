#ifndef APP_MUSIC_H
#define APP_MUSIC_H

#include "app_input.h"

void app_music_open(void);
void app_music_close(void);
void app_music_handle_event(const app_input_event_t *event);
void app_music_update(void);

#endif
