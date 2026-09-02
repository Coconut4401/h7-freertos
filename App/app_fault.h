#ifndef APP_FAULT_H
#define APP_FAULT_H

#include <stdint.h>

typedef enum
{
    APP_FAULT_NONE = 0U,
    APP_FAULT_ASSERT = 1U,
    APP_FAULT_STACK_OVERFLOW = 2U,
    APP_FAULT_MALLOC_FAILED = 3U,
    APP_FAULT_HARDFAULT = 4U,
    APP_FAULT_BUSFAULT = 5U,
    APP_FAULT_USAGEFAULT = 6U,
    APP_FAULT_MEMMANAGE = 7U,
    APP_FAULT_HEALTH_TIMEOUT = 8U
} app_fault_type_t;

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t sequence;
    uint32_t type;
    uint32_t tick;
    uint32_t reset_flags;
    uint32_t cfsr;
    uint32_t hfsr;
    uint32_t dfsr;
    uint32_t afsr;
    uint32_t bfar;
    uint32_t mmfar;
    uint32_t stacked_pc;
    uint32_t stacked_lr;
    uint32_t stacked_sp;
    char task[16];
    uint32_t checksum;
} app_fault_record_t;

void app_fault_init(void);
uint8_t app_fault_get_pending(app_fault_record_t *record);
void app_fault_get_boot_record(app_fault_record_t *record);
void app_fault_clear(void);
void app_fault_reported(void);
void app_fault_publish_pending(void);
void app_fault_record(app_fault_type_t type, const char *task_name);
void app_fault_record_assert(const char *file, uint32_t line);
void app_fault_assert_and_reset(const char *file, uint32_t line);
void app_fault_record_exception(app_fault_type_t type);
void app_fault_panic(app_fault_type_t type, const char *task_name);

#endif
