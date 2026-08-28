#include "app_rtc.h"

#include <stdint.h>
#include <stdio.h>

#include "./BSP/IIC/myiic.h"
#include "FreeRTOS.h"
#include "task.h"

#define DS3231_ADDRESS_WRITE       0xD0U
#define DS3231_ADDRESS_READ        0xD1U
#define DS3231_REG_SECONDS         0x00U
#define DS3231_REG_CAL_SIGNATURE   0x07U
#define DS3231_REG_CONTROL         0x0EU
#define DS3231_REG_STATUS          0x0FU
#define DS3231_STATUS_OSF          0x80U
#define APP_RTC_REFRESH_MS         250U

typedef struct
{
    uint8_t available;
    uint8_t initialized;
    app_rtc_datetime_t calibration_datetime;
    app_rtc_datetime_t current_datetime;
    TickType_t last_refresh_tick;
} app_rtc_state_t;

static app_rtc_state_t g_rtc;

static uint8_t app_rtc_write_registers(uint8_t reg,
                                       const uint8_t *data,
                                       uint8_t length);
static uint8_t app_rtc_read_registers(uint8_t reg,
                                      uint8_t *data,
                                      uint8_t length);
static uint32_t app_rtc_datetime_to_seconds(
    const app_rtc_datetime_t *datetime);
static void app_rtc_seconds_to_datetime(uint32_t total_seconds,
                                        app_rtc_datetime_t *datetime);
static void app_rtc_apply_compensation(
    const app_rtc_datetime_t *raw_datetime,
    app_rtc_datetime_t *corrected_datetime);

static uint32_t app_rtc_build_signature(
    const app_rtc_datetime_t *build_datetime)
{
    return app_rtc_datetime_to_seconds(build_datetime) ^ 0x52544331U;
}

static uint8_t app_rtc_signature_matches(uint32_t expected_signature)
{
    uint8_t data[4];
    uint32_t stored_signature;

    if (!app_rtc_read_registers(DS3231_REG_CAL_SIGNATURE,
                                data, sizeof(data)))
    {
        return 0U;
    }
    stored_signature = (uint32_t)data[0] |
                       ((uint32_t)data[1] << 8U) |
                       ((uint32_t)data[2] << 16U) |
                       ((uint32_t)data[3] << 24U);
    return (stored_signature == expected_signature) ? 1U : 0U;
}

static uint8_t app_rtc_write_signature(uint32_t signature)
{
    uint8_t data[4];

    data[0] = (uint8_t)signature;
    data[1] = (uint8_t)(signature >> 8U);
    data[2] = (uint8_t)(signature >> 16U);
    data[3] = (uint8_t)(signature >> 24U);
    return app_rtc_write_registers(DS3231_REG_CAL_SIGNATURE,
                                   data, sizeof(data));
}

static uint8_t app_rtc_bcd_to_bin(uint8_t value)
{
    return (uint8_t)((value >> 4U) * 10U + (value & 0x0FU));
}

static uint8_t app_rtc_bin_to_bcd(uint8_t value)
{
    return (uint8_t)(((value / 10U) << 4U) | (value % 10U));
}

static uint8_t app_rtc_is_leap_year(uint16_t year)
{
    return ((year % 4U) == 0U) ? 1U : 0U;
}

static uint8_t app_rtc_days_in_month(uint16_t year, uint8_t month)
{
    static const uint8_t days[12] =
    {
        31U, 28U, 31U, 30U, 31U, 30U,
        31U, 31U, 30U, 31U, 30U, 31U
    };

    if (month == 2U && app_rtc_is_leap_year(year))
    {
        return 29U;
    }
    return (month >= 1U && month <= 12U) ? days[month - 1U] : 0U;
}

static uint8_t app_rtc_datetime_is_valid(const app_rtc_datetime_t *datetime)
{
    if (datetime == NULL || datetime->year < 2000U ||
        datetime->year > 2099U || datetime->month < 1U ||
        datetime->month > 12U || datetime->date < 1U ||
        datetime->date > app_rtc_days_in_month(datetime->year,
                                               datetime->month) ||
        datetime->day < 1U || datetime->day > 7U || datetime->hour > 23U ||
        datetime->minute > 59U || datetime->second > 59U)
    {
        return 0U;
    }
    return 1U;
}

static uint8_t app_rtc_write_registers(uint8_t reg,
                                       const uint8_t *data,
                                       uint8_t length)
{
    uint8_t index;

    iic_start();
    iic_send_byte(DS3231_ADDRESS_WRITE);
    if (iic_wait_ack() != 0U)
    {
        iic_stop();
        return 0U;
    }
    iic_send_byte(reg);
    if (iic_wait_ack() != 0U)
    {
        iic_stop();
        return 0U;
    }
    for (index = 0U; index < length; index++)
    {
        iic_send_byte(data[index]);
        if (iic_wait_ack() != 0U)
        {
            iic_stop();
            return 0U;
        }
    }
    iic_stop();
    return 1U;
}

static uint8_t app_rtc_read_registers(uint8_t reg,
                                      uint8_t *data,
                                      uint8_t length)
{
    uint8_t index;

    iic_start();
    iic_send_byte(DS3231_ADDRESS_WRITE);
    if (iic_wait_ack() != 0U)
    {
        iic_stop();
        return 0U;
    }
    iic_send_byte(reg);
    if (iic_wait_ack() != 0U)
    {
        iic_stop();
        return 0U;
    }
    iic_start();
    iic_send_byte(DS3231_ADDRESS_READ);
    if (iic_wait_ack() != 0U)
    {
        iic_stop();
        return 0U;
    }
    for (index = 0U; index < length; index++)
    {
        data[index] = iic_read_byte((index + 1U < length) ? 1U : 0U);
    }
    iic_stop();
    return 1U;
}

static uint8_t app_rtc_read_status(uint8_t *status)
{
    return app_rtc_read_registers(DS3231_REG_STATUS, status, 1U);
}

static uint8_t app_rtc_read_datetime(app_rtc_datetime_t *datetime)
{
    uint8_t data[7];
    uint8_t hour;

    if (!app_rtc_read_registers(DS3231_REG_SECONDS, data, sizeof(data)))
    {
        return 0U;
    }

    hour = data[2];
    if ((hour & 0x40U) != 0U)
    {
        hour = (uint8_t)(app_rtc_bcd_to_bin(hour & 0x1FU) +
                         ((hour & 0x20U) != 0U ? 12U : 0U));
    }
    else
    {
        hour = app_rtc_bcd_to_bin(hour & 0x3FU);
    }

    datetime->second = app_rtc_bcd_to_bin(data[0] & 0x7FU);
    datetime->minute = app_rtc_bcd_to_bin(data[1] & 0x7FU);
    datetime->hour = hour;
    datetime->day = app_rtc_bcd_to_bin(data[3] & 0x07U);
    datetime->date = app_rtc_bcd_to_bin(data[4] & 0x3FU);
    datetime->month = app_rtc_bcd_to_bin(data[5] & 0x1FU);
    datetime->year = (uint16_t)(2000U + app_rtc_bcd_to_bin(data[6]));
    return app_rtc_datetime_is_valid(datetime);
}

static uint8_t app_rtc_write_datetime(const app_rtc_datetime_t *datetime)
{
    uint8_t data[7];

    data[0] = app_rtc_bin_to_bcd(datetime->second);
    data[1] = app_rtc_bin_to_bcd(datetime->minute);
    data[2] = app_rtc_bin_to_bcd(datetime->hour);
    data[3] = app_rtc_bin_to_bcd(datetime->day);
    data[4] = app_rtc_bin_to_bcd(datetime->date);
    data[5] = app_rtc_bin_to_bcd(datetime->month);
    data[6] = app_rtc_bin_to_bcd((uint8_t)(datetime->year - 2000U));
    return app_rtc_write_registers(DS3231_REG_SECONDS, data, sizeof(data));
}

static uint8_t app_rtc_month_from_text(const char *text)
{
    static const char *const months[12] =
    {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };
    uint8_t index;

    for (index = 0U; index < 12U; index++)
    {
        if (text[0] == months[index][0] && text[1] == months[index][1] &&
            text[2] == months[index][2])
        {
            return (uint8_t)(index + 1U);
        }
    }
    return 1U;
}

static uint8_t app_rtc_decimal_pair(const char *text)
{
    return (uint8_t)((text[0] - '0') * 10 + text[1] - '0');
}

static app_rtc_datetime_t app_rtc_build_datetime(void)
{
    app_rtc_datetime_t datetime;

    datetime.year = (uint16_t)((__DATE__[7] - '0') * 1000U +
                               (__DATE__[8] - '0') * 100U +
                               (__DATE__[9] - '0') * 10U +
                               (__DATE__[10] - '0'));
    datetime.month = app_rtc_month_from_text(__DATE__);
    datetime.date = (__DATE__[4] == ' ') ?
        (uint8_t)(__DATE__[5] - '0') : app_rtc_decimal_pair(&__DATE__[4]);
    datetime.day = 1U;
    datetime.hour = app_rtc_decimal_pair(&__TIME__[0]);
    datetime.minute = app_rtc_decimal_pair(&__TIME__[3]);
    datetime.second = app_rtc_decimal_pair(&__TIME__[6]);
    return datetime;
}

void app_rtc_init(void)
{
    uint8_t status;
    uint8_t control;
    uint8_t datetime_valid;
    uint8_t initialize_from_build;
    uint32_t build_signature;
    app_rtc_datetime_t datetime;
    app_rtc_datetime_t build_datetime;

    if (g_rtc.initialized)
    {
        return;
    }
    iic_init();
    g_rtc.initialized = 1U;
    g_rtc.available = 0U;
    if (!app_rtc_read_status(&status))
    {
        printf("DS3231: NO ACK at 0x68\r\n");
        return;
    }

    printf("DS3231: ACK at 0x68\r\n");
    datetime_valid = app_rtc_read_datetime(&datetime);
    build_datetime = app_rtc_build_datetime();
    build_signature = app_rtc_build_signature(&build_datetime);
    initialize_from_build = (!datetime_valid ||
                             (status & DS3231_STATUS_OSF) != 0U) ? 1U : 0U;
    if (!app_rtc_signature_matches(build_signature))
    {
        initialize_from_build = 1U;
        printf("DS3231: new firmware calibration required\r\n");
    }

    if (initialize_from_build)
    {
        datetime = build_datetime;
        if (!app_rtc_write_datetime(&datetime))
        {
            printf("DS3231: time write failed\r\n");
            return;
        }
        if (!app_rtc_write_signature(build_signature))
        {
            printf("DS3231: calibration signature write failed\r\n");
            return;
        }
        status = (uint8_t)(status & (uint8_t)~DS3231_STATUS_OSF);
        if (!app_rtc_write_registers(DS3231_REG_STATUS, &status, 1U))
        {
            return;
        }
        printf("DS3231: initialized from firmware build time\r\n");
    }

    if (app_rtc_read_registers(DS3231_REG_CONTROL, &control, 1U))
    {
        if ((control & 0x80U) != 0U)
        {
            control = (uint8_t)(control & (uint8_t)~0x80U);
            (void)app_rtc_write_registers(DS3231_REG_CONTROL, &control, 1U);
        }
    }

    g_rtc.calibration_datetime = build_datetime;
    g_rtc.current_datetime = datetime;
    g_rtc.last_refresh_tick = xTaskGetTickCount();
    g_rtc.available = 1U;
    printf("DS3231 time base: %04u-%02u-%02u %02u:%02u:%02u, compensation=%d ppm\r\n",
           datetime.year, datetime.month, datetime.date,
           datetime.hour, datetime.minute, datetime.second,
           APP_RTC_COMPENSATION_PPM);
}

uint8_t app_rtc_is_available(void)
{
    return g_rtc.available;
}

void app_rtc_update(void)
{
    TickType_t now;
    app_rtc_datetime_t raw_datetime;

    if (!g_rtc.available)
    {
        return;
    }
    now = xTaskGetTickCount();
    if ((TickType_t)(now - g_rtc.last_refresh_tick) <
        pdMS_TO_TICKS(APP_RTC_REFRESH_MS))
    {
        return;
    }
    g_rtc.last_refresh_tick = now;
    if (app_rtc_read_datetime(&raw_datetime))
    {
        app_rtc_apply_compensation(&raw_datetime,
                                   &g_rtc.current_datetime);
    }
}

static uint32_t app_rtc_days_before_year(uint16_t year)
{
    uint32_t days;
    uint16_t current;

    days = 0U;
    for (current = 2000U; current < year; current++)
    {
        days += app_rtc_is_leap_year(current) ? 366U : 365U;
    }
    return days;
}

static uint32_t app_rtc_datetime_to_seconds(const app_rtc_datetime_t *datetime)
{
    uint32_t days;
    uint8_t month;

    days = app_rtc_days_before_year(datetime->year);
    for (month = 1U; month < datetime->month; month++)
    {
        days += app_rtc_days_in_month(datetime->year, month);
    }
    days += (uint32_t)(datetime->date - 1U);
    return days * 86400U + (uint32_t)datetime->hour * 3600U +
           (uint32_t)datetime->minute * 60U + datetime->second;
}

static void app_rtc_seconds_to_datetime(uint32_t total_seconds,
                                        app_rtc_datetime_t *datetime)
{
    uint32_t days;

    days = total_seconds / 86400U;
    total_seconds %= 86400U;
    datetime->hour = (uint8_t)(total_seconds / 3600U);
    total_seconds %= 3600U;
    datetime->minute = (uint8_t)(total_seconds / 60U);
    datetime->second = (uint8_t)(total_seconds % 60U);
    datetime->year = 2000U;
    while (days >= (app_rtc_is_leap_year(datetime->year) ? 366U : 365U))
    {
        days -= app_rtc_is_leap_year(datetime->year) ? 366U : 365U;
        datetime->year++;
    }
    datetime->month = 1U;
    while (days >= app_rtc_days_in_month(datetime->year, datetime->month))
    {
        days -= app_rtc_days_in_month(datetime->year, datetime->month);
        datetime->month++;
    }
    datetime->date = (uint8_t)(days + 1U);
    datetime->day = 1U;
}

static void app_rtc_apply_compensation(
    const app_rtc_datetime_t *raw_datetime,
    app_rtc_datetime_t *corrected_datetime)
{
    uint32_t calibration_seconds;
    uint32_t raw_seconds;
    uint32_t elapsed_seconds;
    int64_t corrected_elapsed;

    calibration_seconds =
        app_rtc_datetime_to_seconds(&g_rtc.calibration_datetime);
    raw_seconds = app_rtc_datetime_to_seconds(raw_datetime);
    if (raw_seconds < calibration_seconds)
    {
        *corrected_datetime = *raw_datetime;
        return;
    }

    elapsed_seconds = raw_seconds - calibration_seconds;
    corrected_elapsed = (int64_t)elapsed_seconds +
        ((int64_t)elapsed_seconds * APP_RTC_COMPENSATION_PPM) / 1000000LL;
    app_rtc_seconds_to_datetime(
        calibration_seconds + (uint32_t)corrected_elapsed,
        corrected_datetime);
}

uint8_t app_rtc_get_datetime(app_rtc_datetime_t *datetime)
{
    if (!g_rtc.available || datetime == NULL)
    {
        return 0U;
    }
    *datetime = g_rtc.current_datetime;
    return 1U;
}

uint32_t app_rtc_get_seconds_of_day(void)
{
    app_rtc_datetime_t datetime;

    if (!app_rtc_get_datetime(&datetime))
    {
        return (uint32_t)(xTaskGetTickCount() / configTICK_RATE_HZ);
    }
    return (uint32_t)datetime.hour * 3600U +
           (uint32_t)datetime.minute * 60U + datetime.second;
}
