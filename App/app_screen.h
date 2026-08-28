#ifndef APP_SCREEN_H
#define APP_SCREEN_H

#include <stdint.h>

void app_screen_init(void);
void app_screen_off(void);
void app_screen_on(void);
uint8_t app_screen_is_off(void);
void app_screen_set_brightness(uint8_t brightness_percent);
uint8_t app_screen_get_brightness(void);

#endif
