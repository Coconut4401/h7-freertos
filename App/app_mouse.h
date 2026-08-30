#ifndef APP_MOUSE_H
#define APP_MOUSE_H

#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "app_input.h"
#include "./BSP/CH9350/ch9350.h"

void app_mouse_init(QueueHandle_t event_queue);
void app_mouse_set_position(uint16_t x, uint16_t y);
void app_mouse_process_report(const ch9350_mouse_report_t *report);
void app_mouse_disconnect(void);
void app_mouse_flush(void);

#endif
