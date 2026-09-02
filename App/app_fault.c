#include "app_fault.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "stm32h743xx.h"
#include "app_logs.h"
#include "app_storage.h"

#define APP_FAULT_MAGIC        0x46544C31UL
#define APP_FAULT_VERSION      1U
#define APP_FAULT_BKPSRAM      0x38800000UL
#define APP_FAULT_RESET_MASK   0x7FFE0000UL
#define APP_FAULT_LOG_NAME     "CRASH.LOG"
#define APP_FAULT_RETRY_MS     5000U

static uint8_t g_fault_submit_pending;
static uint8_t g_fault_logged;
static TickType_t g_fault_last_attempt;
static char g_fault_log_buffer[512];
static app_fault_record_t g_boot_record;

static volatile app_fault_record_t *app_fault_storage(void)
{
    return (volatile app_fault_record_t *)APP_FAULT_BKPSRAM;
}

static uint32_t app_fault_checksum(const app_fault_record_t *record)
{
    const uint8_t *bytes;
    uint32_t hash;
    uint32_t index;

    bytes = (const uint8_t *)record;
    hash = 2166136261UL;
    for (index = 0U; index < offsetof(app_fault_record_t, checksum); index++)
    {
        hash ^= bytes[index];
        hash *= 16777619UL;
    }
    return hash;
}

static void app_fault_flush_storage(void)
{
    SCB_CleanDCache_by_Addr((uint32_t *)app_fault_storage(),
        (int32_t)((sizeof(app_fault_record_t) + 31U) & ~31U));
    __DSB();
}

static void app_fault_copy_task(volatile char *destination,
                                const char *source)
{
    uint32_t index;

    for (index = 0U; index < 16U; index++)
    {
        destination[index] = '\0';
    }
    for (index = 0U; index < 15U; index++)
    {
        destination[index] = (source != NULL && source[index] != '\0') ?
            source[index] : '\0';
        if (destination[index] == '\0')
        {
            break;
        }
    }
}

void app_fault_init(void)
{
    app_fault_record_t record;
    volatile app_fault_record_t *stored;

    RCC->AHB4ENR |= RCC_AHB4ENR_BKPRAMEN;
    __DSB();
    stored = app_fault_storage();
    memset(&g_boot_record, 0, sizeof(g_boot_record));
    g_boot_record.reset_flags = RCC->RSR & APP_FAULT_RESET_MASK;
    if (app_fault_get_pending(&record))
    {
        record.reset_flags = RCC->RSR & APP_FAULT_RESET_MASK;
        record.checksum = app_fault_checksum(&record);
        *stored = record;
        app_fault_flush_storage();
        g_boot_record = record;
    }
    RCC->RSR |= RCC_RSR_RMVF;
    g_fault_submit_pending = 0U;
    g_fault_logged = 0U;
    g_fault_last_attempt = 0U;
}

void app_fault_get_boot_record(app_fault_record_t *record)
{
    if (record != NULL)
    {
        *record = g_boot_record;
    }
}

uint8_t app_fault_get_pending(app_fault_record_t *record)
{
    volatile app_fault_record_t *stored;
    app_fault_record_t copy;

    stored = app_fault_storage();
    copy = *stored;
    if (copy.magic != APP_FAULT_MAGIC ||
        copy.version != APP_FAULT_VERSION ||
        copy.size != sizeof(app_fault_record_t) ||
        copy.checksum != app_fault_checksum(&copy))
    {
        return 0U;
    }
    if (record != NULL)
    {
        *record = copy;
    }
    return 1U;
}

void app_fault_clear(void)
{
    volatile app_fault_record_t *stored;

    stored = app_fault_storage();
    stored->magic = 0U;
    app_fault_flush_storage();
}

void app_fault_record(app_fault_type_t type, const char *task_name)
{
    volatile app_fault_record_t *stored;
    app_fault_record_t record;

    stored = app_fault_storage();
    record.magic = APP_FAULT_MAGIC;
    record.version = APP_FAULT_VERSION;
    record.size = sizeof(app_fault_record_t);
    record.sequence = (stored->magic == APP_FAULT_MAGIC) ?
        stored->sequence + 1U : 1U;
    record.type = (uint32_t)type;
    record.tick = (__get_IPSR() == 0U &&
                   xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) ?
        (uint32_t)xTaskGetTickCount() : 0U;
    record.reset_flags = RCC->RSR & APP_FAULT_RESET_MASK;
    record.cfsr = SCB->CFSR;
    record.hfsr = SCB->HFSR;
    record.dfsr = SCB->DFSR;
    record.afsr = SCB->AFSR;
    record.bfar = SCB->BFAR;
    record.mmfar = SCB->MMFAR;
    record.stacked_pc = 0U;
    record.stacked_lr = 0U;
    record.stacked_sp = 0U;
    app_fault_copy_task(record.task, task_name);
    record.checksum = app_fault_checksum(&record);
    *stored = record;
    app_fault_flush_storage();
    __ISB();
}

void app_fault_record_assert(const char *file, uint32_t line)
{
    app_fault_record_t record;
    volatile app_fault_record_t *stored;
    const char *name;
    const char *cursor;

    name = (file != NULL) ? file : "ASSERT";
    cursor = name;
    while (*cursor != '\0')
    {
        if (*cursor == '/' || *cursor == '\\')
        {
            name = cursor + 1;
        }
        cursor++;
    }
    app_fault_record(APP_FAULT_ASSERT, name);
    stored = app_fault_storage();
    record = *stored;
    record.stacked_pc = line;
    record.checksum = app_fault_checksum(&record);
    *stored = record;
    app_fault_flush_storage();
}

void app_fault_assert_and_reset(const char *file, uint32_t line)
{
    app_fault_record_assert(file, line);
    NVIC_SystemReset();
    for (;;)
    {
    }
}

void app_fault_reported(void)
{
    app_fault_clear();
}

void app_fault_publish_pending(void)
{
    app_fault_record_t record;
    app_storage_request_t request;
    app_storage_binary_response_t response;
    TickType_t now;
    int length;

    while (app_storage_receive_fault(&response) == pdPASS)
    {
        g_fault_submit_pending = 0U;
        if (response.result == APP_STORAGE_RESULT_OK)
        {
            app_logs_add(APP_LOG_LEVEL_INFO, "FAULT",
                         "CRASH.LOG SAVED TO SD CARD");
            app_fault_reported();
            g_fault_logged = 1U;
        }
        else
        {
            app_logs_add(APP_LOG_LEVEL_ERROR, "FAULT",
                         "CRASH.LOG SAVE FAILED");
        }
    }
    if (g_fault_logged || g_fault_submit_pending ||
        !app_fault_get_pending(&record))
    {
        return;
    }

    now = xTaskGetTickCount();
    if (g_fault_last_attempt != 0U &&
        (TickType_t)(now - g_fault_last_attempt) <
        pdMS_TO_TICKS(APP_FAULT_RETRY_MS))
    {
        return;
    }
    g_fault_last_attempt = now;
    length = snprintf(g_fault_log_buffer, sizeof(g_fault_log_buffer),
        "STM32H743 CRASH RECORD\r\n"
        "SEQ=%lu TYPE=%lu TASK=%s TICK=%lu RESET=0x%08lX\r\n"
        "CFSR=0x%08lX HFSR=0x%08lX DFSR=0x%08lX AFSR=0x%08lX\r\n"
        "BFAR=0x%08lX MMFAR=0x%08lX PC=0x%08lX LR=0x%08lX SP=0x%08lX\r\n",
        (unsigned long)record.sequence,
        (unsigned long)record.type,
        record.task,
        (unsigned long)record.tick,
        (unsigned long)record.reset_flags,
        (unsigned long)record.cfsr,
        (unsigned long)record.hfsr,
        (unsigned long)record.dfsr,
        (unsigned long)record.afsr,
        (unsigned long)record.bfar,
        (unsigned long)record.mmfar,
        (unsigned long)record.stacked_pc,
        (unsigned long)record.stacked_lr,
        (unsigned long)record.stacked_sp);
    if (length <= 0 || (uint32_t)length >= sizeof(g_fault_log_buffer))
    {
        return;
    }

    memset(&request, 0, sizeof(request));
    request.operation = APP_STORAGE_OP_WRITE_FAULT;
    memcpy(request.name, APP_FAULT_LOG_NAME, sizeof(APP_FAULT_LOG_NAME));
    request.binary_data = g_fault_log_buffer;
    request.binary_length = (uint32_t)length;
    if (app_storage_submit(&request) == pdPASS)
    {
        g_fault_submit_pending = 1U;
    }
}

void app_fault_record_exception(app_fault_type_t type)
{
    app_fault_record(type, "EXCEPTION");
}

__attribute__((used, noinline, noreturn))
void app_fault_exception_frame(uint32_t *stack,
                               uint32_t exception_return,
                               uint32_t type)
{
    app_fault_record_t record;
    volatile app_fault_record_t *stored;

    app_fault_record((app_fault_type_t)type, "EXCEPTION");
    stored = app_fault_storage();
    record = *stored;
    if (stack != NULL)
    {
        if ((exception_return & (1UL << 4)) == 0U)
        {
            stack += 18U;
        }
        record.stacked_lr = stack[5];
        record.stacked_pc = stack[6];
        record.stacked_sp = (uint32_t)stack;
    }
    record.checksum = app_fault_checksum(&record);
    *stored = record;
    app_fault_flush_storage();
    NVIC_SystemReset();
    for (;;)
    {
    }
}

void app_fault_panic(app_fault_type_t type, const char *task_name)
{
    app_fault_record(type, task_name);
    NVIC_SystemReset();
    for (;;)
    {
    }
}

void vApplicationStackOverflowHook(TaskHandle_t task, char *task_name)
{
    (void)task;
    app_fault_panic(APP_FAULT_STACK_OVERFLOW, task_name);
}

void vApplicationMallocFailedHook(void)
{
    app_fault_panic(APP_FAULT_MALLOC_FAILED, "MALLOC");
}

__attribute__((naked)) void HardFault_Handler(void)
{
    __asm volatile(
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "mov r1, lr\n"
        "movs r2, #4\n"
        "b app_fault_exception_frame\n");
}

__attribute__((naked)) void BusFault_Handler(void)
{
    __asm volatile(
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "mov r1, lr\n"
        "movs r2, #5\n"
        "b app_fault_exception_frame\n");
}

__attribute__((naked)) void UsageFault_Handler(void)
{
    __asm volatile(
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "mov r1, lr\n"
        "movs r2, #6\n"
        "b app_fault_exception_frame\n");
}
