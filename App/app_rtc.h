#ifndef APP_RTC_H
#define APP_RTC_H

#include <stdint.h>

/* Positive values make the software clock run faster; negative values make it slower. */
#define APP_RTC_COMPENSATION_PPM    0

typedef struct
{
    uint16_t year;
    uint8_t month;
    uint8_t date;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} app_rtc_datetime_t;

void app_rtc_init(void);
uint8_t app_rtc_is_available(void);
void app_rtc_update(void);
uint8_t app_rtc_get_datetime(app_rtc_datetime_t *datetime);
uint8_t app_rtc_set_datetime(const app_rtc_datetime_t *datetime);
uint32_t app_rtc_get_seconds_of_day(void);

#endif
