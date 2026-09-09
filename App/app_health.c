/**
 * @file app_health.c
 * @brief 周期检查任务与外设健康状态，并更新系统健康统计。
 * @details 这是 app_health 模块的实现文件（App/app_health.c）。调用本模块接口时，应遵守
 *          相应外设初始化顺序、缓冲区有效期和 FreeRTOS 任务上下文约束。
 * @note 文件采用 UTF-8 编码；硬件资源分配以板级原理图和工程配置为准。
 */

#include "app_health.h"

#include <stddef.h>

#include "app_fault.h"
#include "stm32h743xx.h"
#include "timers.h"

/** @name 编译期配置与硬件参数：集中定义本模块使用的常量和宏。 */
#define APP_HEALTH_REQUIRED_MASK ((1UL << APP_HEALTH_COUNT) - 1UL)
#define APP_HEALTH_PERIOD_MS     250U
#define APP_HEALTH_STARTUP_MS    15000U
#define APP_HEALTH_INPUT_MAX_MS  1500U
#define APP_HEALTH_RUNTIME_MAX_MS 1500U
#define APP_HEALTH_STORAGE_MAX_MS 10000U
#define APP_HEALTH_AUDIO_MAX_MS  5000U
#define APP_HEALTH_MONITOR_MAX_MS 2000U
#define APP_HEALTH_WATCHDOG_RELOAD 1875U
#define APP_HEALTH_LSI_READY_MS      100U
#define APP_HEALTH_IWDG_UPDATE_MS    7000U
#define APP_HEALTH_WATCHDOG_RETRY_MS 5000U
#define APP_HEALTH_STACK_WARNING_WORDS 128U

static volatile uint32_t g_beat_mask;
static volatile TickType_t g_last_beat[APP_HEALTH_COUNT];
static app_health_snapshot_t g_snapshot;
static uint8_t g_watchdog_enabled;
static TickType_t g_watchdog_last_attempt;

/**
 * @brief app_health_timeout_ms：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param id 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint32_t app_health_timeout_ms(app_health_task_id_t id)
{
    static const uint32_t limits[APP_HEALTH_COUNT] =
    {
        APP_HEALTH_INPUT_MAX_MS,
        APP_HEALTH_RUNTIME_MAX_MS,
        APP_HEALTH_STORAGE_MAX_MS,
        APP_HEALTH_AUDIO_MAX_MS,
        APP_HEALTH_MONITOR_MAX_MS
    };
    return limits[id];
}

/**
 * @brief app_health_task_handle：作为 FreeRTOS 任务入口，循环处理事件、周期工作和运行状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param context 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param id 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 * @warning 该入口具有特定中断或任务上下文，禁止执行不符合该上下文约束的操作。
 */
static TaskHandle_t app_health_task_handle(const app_health_context_t *context,
                                           app_health_task_id_t id)
{
    static const uint8_t unused = 0U;
    (void)unused;
    switch (id)
    {
        case APP_HEALTH_INPUT: return context->input_task;
        case APP_HEALTH_RUNTIME: return context->runtime_task;
        case APP_HEALTH_STORAGE: return context->storage_task;
        case APP_HEALTH_AUDIO: return context->audio_task;
        case APP_HEALTH_MONITOR: return context->monitor_task;
        default: return NULL;
    }
}

/**
 * @brief app_health_wait_bits：等待指定时长或硬件条件，以满足总线时序与同步要求。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param reg 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param mask 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @param wait_for_set 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint8_t app_health_wait_bits(volatile uint32_t *reg,
                                    uint32_t mask,
                                    uint8_t wait_for_set,
                                    uint32_t timeout_ms)
{
    TickType_t started;
    uint8_t matched;

    started = xTaskGetTickCount();
    do
    {
        matched = ((*reg & mask) != 0U) ? 1U : 0U;
        if (matched == wait_for_set)
        {
            return 1U;
        }
        vTaskDelay(pdMS_TO_TICKS(1U));
    } while ((TickType_t)(xTaskGetTickCount() - started) <
             pdMS_TO_TICKS(timeout_ms));
    return 0U;
}

/**
 * @brief app_health_iwdg_start：启动或启用函数名所描述的硬件功能与业务流程。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 返回处理结果、状态码或查询值；调用方应按接口语义判断成功与失败。
 */
static uint8_t app_health_iwdg_start(void)
{
    RCC->CSR |= RCC_CSR_LSION;
    if (!app_health_wait_bits(&RCC->CSR, RCC_CSR_LSIRDY, 1U,
                              APP_HEALTH_LSI_READY_MS))
    {
        return 0U;
    }

    /*
     * STM32H7 requires the IWDG to be running before PR/RLR updates are
     * synchronized into the watchdog clock domain.  Starting it after the
     * PVU/RVU wait leaves both flags pending forever and WDG stays OFF.
     */
    DBGMCU->APB4FZ1 |= DBGMCU_APB4FZ1_DBG_IWDG1;
    IWDG1->KR = 0xCCCCU;
    IWDG1->KR = 0x5555U;
    IWDG1->PR = 6U;
    IWDG1->RLR = APP_HEALTH_WATCHDOG_RELOAD;
    if (!app_health_wait_bits(&IWDG1->SR,
                              IWDG_SR_PVU | IWDG_SR_RVU, 0U,
                              APP_HEALTH_IWDG_UPDATE_MS))
    {
        return 0U;
    }
    IWDG1->KR = 0xAAAAU;
    g_watchdog_enabled = 1U;
    return 1U;
}

/**
 * @brief app_health_iwdg_feed：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
static void app_health_iwdg_feed(void)
{
    IWDG1->KR = 0xAAAAU;
}

/**
 * @brief app_health_init：按依赖顺序配置硬件或模块状态，为后续访问建立有效运行环境。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @return 无返回值。
 */
void app_health_init(void)
{
    uint32_t index;

    g_beat_mask = 0U;
    for (index = 0U; index < APP_HEALTH_COUNT; index++)
    {
        g_last_beat[index] = 0U;
    }
    g_watchdog_enabled = 0U;
    g_watchdog_last_attempt = 0U;
    g_snapshot.beat_mask = 0U;
    g_snapshot.healthy_mask = 0U;
    g_snapshot.timeout_mask = 0U;
    g_snapshot.current_heap = 0U;
    g_snapshot.minimum_heap = 0U;
    g_snapshot.watchdog_enabled = 0U;
    g_snapshot.startup_complete = 0U;
}

/**
 * @brief app_health_beat：完成该接口负责的模块操作，并保持相关硬件与软件状态一致。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param task_id 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_health_beat(app_health_task_id_t task_id)
{
    if (task_id < APP_HEALTH_COUNT)
    {
        g_last_beat[task_id] = xTaskGetTickCount();
        g_beat_mask |= 1UL << task_id;
    }
}

/**
 * @brief app_health_get_snapshot：读取指定寄存器、缓冲区或模块状态，并把结果提供给调用方。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param snapshot 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 */
void app_health_get_snapshot(app_health_snapshot_t *snapshot)
{
    uint32_t index;

    if (snapshot == NULL)
    {
        return;
    }
    taskENTER_CRITICAL();
    *snapshot = g_snapshot;
    for (index = 0U; index < APP_HEALTH_COUNT; index++)
    {
        snapshot->stack_words[index] = g_snapshot.stack_words[index];
    }
    taskEXIT_CRITICAL();
}

/**
 * @brief AppHealthTask：作为 FreeRTOS 任务入口，循环处理事件、周期工作和运行状态。
 * @details 此处为接口实现；执行顺序沿用模块既有设计。涉及共享状态时，调用方需保证
 *          初始化已经完成，并避免与中断或其他任务产生未受控的并发访问。
 * @param argument 调用方提供的输入或输出参数；其取值范围和缓冲区有效期须符合接口约定。
 * @return 无返回值。
 * @warning 该入口具有特定中断或任务上下文，禁止执行不符合该上下文约束的操作。
 */
void AppHealthTask(void *argument)
{
    const app_health_context_t *context;
    TickType_t started;
    TickType_t now;
    uint32_t healthy_mask;
    uint32_t timeout_mask;
    uint32_t low_stack_mask;
    uint32_t index;
    TaskHandle_t task;

    context = (const app_health_context_t *)argument;
    started = xTaskGetTickCount();
    while (1)
    {
        now = xTaskGetTickCount();
        healthy_mask = 0U;
        timeout_mask = 0U;
        low_stack_mask = 0U;
        for (index = 0U; index < APP_HEALTH_COUNT; index++)
        {
            task = app_health_task_handle(context,
                                          (app_health_task_id_t)index);
            g_snapshot.stack_words[index] = (task != NULL) ?
                (uint32_t)uxTaskGetStackHighWaterMark(task) : 0U;
            if (g_snapshot.stack_words[index] != 0U &&
                g_snapshot.stack_words[index] <
                APP_HEALTH_STACK_WARNING_WORDS)
            {
                low_stack_mask |= 1UL << index;
            }
            if ((g_beat_mask & (1UL << index)) != 0U &&
                (uint32_t)(now - g_last_beat[index]) <=
                pdMS_TO_TICKS(app_health_timeout_ms(
                    (app_health_task_id_t)index)))
            {
                healthy_mask |= 1UL << index;
            }
            else if ((g_snapshot.startup_complete != 0U) ||
                     (uint32_t)(now - started) >
                     pdMS_TO_TICKS(APP_HEALTH_STARTUP_MS))
            {
                timeout_mask |= 1UL << index;
            }
        }

        g_snapshot.beat_mask = g_beat_mask;
        g_snapshot.healthy_mask = healthy_mask;
        g_snapshot.timeout_mask = timeout_mask;
        g_snapshot.current_heap = (uint32_t)xPortGetFreeHeapSize();
        g_snapshot.minimum_heap =
            (uint32_t)xPortGetMinimumEverFreeHeapSize();
        g_snapshot.health_stack_words =
            (uint32_t)uxTaskGetStackHighWaterMark(NULL);
        task = xTimerGetTimerDaemonTaskHandle();
        g_snapshot.timer_stack_words = (task != NULL) ?
            (uint32_t)uxTaskGetStackHighWaterMark(task) : 0U;
        if (g_snapshot.health_stack_words < APP_HEALTH_STACK_WARNING_WORDS)
        {
            low_stack_mask |= 1UL << APP_HEALTH_COUNT;
        }
        if (g_snapshot.timer_stack_words != 0U &&
            g_snapshot.timer_stack_words < APP_HEALTH_STACK_WARNING_WORDS)
        {
            low_stack_mask |= 1UL << (APP_HEALTH_COUNT + 1U);
        }
        g_snapshot.low_stack_mask = low_stack_mask;
        if (healthy_mask == APP_HEALTH_REQUIRED_MASK)
        {
            g_snapshot.startup_complete = 1U;
            if (!g_watchdog_enabled &&
                (g_watchdog_last_attempt == 0U ||
                 (TickType_t)(now - g_watchdog_last_attempt) >=
                 pdMS_TO_TICKS(APP_HEALTH_WATCHDOG_RETRY_MS)))
            {
                g_watchdog_last_attempt = now;
                (void)app_health_iwdg_start();
            }
        }
        g_snapshot.watchdog_enabled = g_watchdog_enabled;

        if (g_watchdog_enabled && timeout_mask != 0U)
        {
            static const char *names[APP_HEALTH_COUNT] =
            {
                "InputTask", "GuiTask", "StorageTask",
                "AudioTask", "MonitorTask"
            };
            for (index = 0U; index < APP_HEALTH_COUNT; index++)
            {
                if ((timeout_mask & (1UL << index)) != 0U)
                {
                    app_fault_record(APP_FAULT_HEALTH_TIMEOUT,
                                     names[index]);
                    break;
                }
            }
            for (;;)
            {
                vTaskDelay(pdMS_TO_TICKS(100U));
            }
        }
        if (g_watchdog_enabled)
        {
            app_health_iwdg_feed();
        }
        vTaskDelay(pdMS_TO_TICKS(APP_HEALTH_PERIOD_MS));
    }
}
