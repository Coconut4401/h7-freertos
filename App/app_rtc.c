/**
 * @file app_rtc.c
 * @brief 访问 RTC 日历和时间寄存器，提供时间设置、读取和格式化能力。
 * @details 这是 app_rtc 模块的实现文件（App/app_rtc.c）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#include "app_rtc.h"

#include <stdint.h>
#include <stdio.h>

#include "./BSP/IIC/myiic.h"
#include "FreeRTOS.h"
#include "task.h"

/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define DS3231_ADDRESS_WRITE       0xD0U
#define DS3231_ADDRESS_READ        0xD1U
#define DS3231_REG_SECONDS         0x00U
#define DS3231_REG_CONTROL         0x0EU
#define DS3231_REG_STATUS          0x0FU
#define DS3231_STATUS_OSF          0x80U
#define APP_RTC_REFRESH_MS         250U

/** @brief 模块数据类型：描述本模块维护的状态、配置或数据快照。 */
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

/**
 * @brief app_rtc_bcd_to_bin：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param value 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint8_t app_rtc_bcd_to_bin(uint8_t value)
{
    return (uint8_t)((value >> 4U) * 10U + (value & 0x0FU));
}

/**
 * @brief app_rtc_bin_to_bcd：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param value 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint8_t app_rtc_bin_to_bcd(uint8_t value)
{
    return (uint8_t)(((value / 10U) << 4U) | (value % 10U));
}

/**
 * @brief app_rtc_is_leap_year：检查函数名所描述的条件是否成立，并返回明确的判断结果。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param year 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint8_t app_rtc_is_leap_year(uint16_t year)
{
    return ((year % 4U) == 0U) ? 1U : 0U;
}

/**
 * @brief app_rtc_days_in_month：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param year 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param month 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
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

/**
 * @brief app_rtc_datetime_is_valid：检查函数名所描述的条件是否成立，并返回明确的判断结果。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param datetime 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
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

/**
 * @brief app_rtc_write_registers：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param reg 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param data 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param length 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
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

/**
 * @brief app_rtc_read_registers：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param reg 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param data 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param length 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
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

/**
 * @brief app_rtc_read_status：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param status 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint8_t app_rtc_read_status(uint8_t *status)
{
    return app_rtc_read_registers(DS3231_REG_STATUS, status, 1U);
}

/**
 * @brief app_rtc_read_datetime：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param datetime 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
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

/**
 * @brief app_rtc_write_datetime：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param datetime 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
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

/**
 * @brief app_rtc_month_from_text：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param text 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
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

/**
 * @brief app_rtc_decimal_pair：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param text 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint8_t app_rtc_decimal_pair(const char *text)
{
    return (uint8_t)((text[0] - '0') * 10 + text[1] - '0');
}

/**
 * @brief app_rtc_build_datetime：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
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
    datetime.day = (uint8_t)(((app_rtc_datetime_to_seconds(&datetime) /
                               86400U + 5U) % 7U) + 1U);
    return datetime;
}

/**
 * @brief app_rtc_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_rtc_init(void)
{
    uint8_t status;
    uint8_t control;
    uint8_t datetime_valid;
    uint8_t initialize_from_build;
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
    initialize_from_build = (!datetime_valid ||
                             (status & DS3231_STATUS_OSF) != 0U) ? 1U : 0U;

    if (initialize_from_build)
    {
        datetime = build_datetime;
        if (!app_rtc_write_datetime(&datetime))
        {
            printf("DS3231: time write failed\r\n");
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

    g_rtc.calibration_datetime = datetime;
    g_rtc.current_datetime = datetime;
    g_rtc.last_refresh_tick = xTaskGetTickCount();
    g_rtc.available = 1U;
    printf("DS3231 time base: %04u-%02u-%02u %02u:%02u:%02u, compensation=%d ppm\r\n",
           datetime.year, datetime.month, datetime.date,
           datetime.hour, datetime.minute, datetime.second,
           APP_RTC_COMPENSATION_PPM);
}

/**
 * @brief app_rtc_is_available：检查函数名所描述的条件是否成立，并返回明确的判断结果。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t app_rtc_is_available(void)
{
    return g_rtc.available;
}

/**
 * @brief app_rtc_update：使用最新数据更新缓存、硬件输出或界面显示状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
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

/**
 * @brief app_rtc_days_before_year：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param year 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
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

/**
 * @brief app_rtc_datetime_to_seconds：将输入值转换为调用方所需的数据格式或表示形式。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param datetime 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
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

/**
 * @brief app_rtc_seconds_to_datetime：将输入值转换为调用方所需的数据格式或表示形式。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param total_seconds 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param datetime 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
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

/**
 * @brief app_rtc_apply_compensation：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param raw_datetime 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param corrected_datetime 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
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

/**
 * @brief app_rtc_get_datetime：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param datetime 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t app_rtc_get_datetime(app_rtc_datetime_t *datetime)
{
    if (!g_rtc.available || datetime == NULL)
    {
        return 0U;
    }
    *datetime = g_rtc.current_datetime;
    return 1U;
}

/**
 * @brief app_rtc_set_datetime：把调用方数据写入目标寄存器、缓冲区或模块状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param datetime 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
uint8_t app_rtc_set_datetime(const app_rtc_datetime_t *datetime)
{
    app_rtc_datetime_t value;
    uint8_t status;

    if (!g_rtc.available || datetime == NULL)
    {
        return 0U;
    }

    value = *datetime;
    value.day = 1U;
    if (!app_rtc_datetime_is_valid(&value))
    {
        return 0U;
    }
    value.day = (uint8_t)(((app_rtc_datetime_to_seconds(&value) /
                           86400U + 5U) % 7U) + 1U);
    if (!app_rtc_write_datetime(&value))
    {
        return 0U;
    }

    if (app_rtc_read_status(&status))
    {
        status = (uint8_t)(status & (uint8_t)~DS3231_STATUS_OSF);
        (void)app_rtc_write_registers(DS3231_REG_STATUS, &status, 1U);
    }

    g_rtc.calibration_datetime = value;
    g_rtc.current_datetime = value;
    g_rtc.last_refresh_tick = xTaskGetTickCount();
    return 1U;
}

/**
 * @brief app_rtc_get_seconds_of_day：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
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
