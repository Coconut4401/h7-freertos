#ifndef APP_FILES_H
#define APP_FILES_H

#include "app_input.h"

void app_files_open(void);
void app_files_close(void);
void app_files_handle_event(const app_input_event_t *event);
void app_files_update(void);

#endif
