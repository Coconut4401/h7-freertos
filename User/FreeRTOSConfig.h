#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* STM32H743 CPU运行频率 */
#define configCPU_CLOCK_HZ                     400000000UL
#define configTICK_RATE_HZ                     1000U

#define configUSE_PREEMPTION                   1
#define configUSE_TIME_SLICING                 1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configUSE_TICKLESS_IDLE                0
#define configIDLE_SHOULD_YIELD                1

#define configMAX_PRIORITIES                   8
#define configMINIMAL_STACK_SIZE               256U
#define configMAX_TASK_NAME_LEN                16
#define configTICK_TYPE_WIDTH_IN_BITS          TICK_TYPE_WIDTH_32_BITS

#define configSUPPORT_DYNAMIC_ALLOCATION       1
#define configSUPPORT_STATIC_ALLOCATION        0
#define configTOTAL_HEAP_SIZE                  (128U * 1024U)

#define configUSE_IDLE_HOOK                    0
#define configUSE_TICK_HOOK                    0
#define configUSE_MALLOC_FAILED_HOOK           0
#define configCHECK_FOR_STACK_OVERFLOW         0

#define configUSE_MUTEXES                      1
#define configUSE_RECURSIVE_MUTEXES            1
#define configUSE_COUNTING_SEMAPHORES          1
#define configUSE_QUEUE_SETS                   1
#define configQUEUE_REGISTRY_SIZE              8

#define configUSE_TASK_NOTIFICATIONS           1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES  1

#define configUSE_CO_ROUTINES                  0
#define configMAX_CO_ROUTINE_PRIORITIES        2

#define configUSE_TIMERS                       1
#define configTIMER_TASK_PRIORITY              2
#define configTIMER_QUEUE_LENGTH               10
#define configTIMER_TASK_STACK_DEPTH           512U

#define configUSE_TRACE_FACILITY               1
#define configUSE_STATS_FORMATTING_FUNCTIONS   1
#define configGENERATE_RUN_TIME_STATS          0

#define INCLUDE_vTaskDelay                     1
#define INCLUDE_vTaskDelayUntil                1
#define INCLUDE_vTaskDelete                    1
#define INCLUDE_vTaskSuspend                   1
#define INCLUDE_vTaskPrioritySet               1
#define INCLUDE_uxTaskPriorityGet              1
#define INCLUDE_xTaskGetSchedulerState         1
#define INCLUDE_xTaskGetCurrentTaskHandle      1
#define INCLUDE_uxTaskGetStackHighWaterMark    1

/* STM32H743具有4个中断优先级位 */
#define configPRIO_BITS                        4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY        15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY   5

#define configKERNEL_INTERRUPT_PRIORITY        \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

#define configMAX_SYSCALL_INTERRUPT_PRIORITY   \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/* 使用STM32启动文件中的标准中断名称 */
#define vPortSVCHandler                        SVC_Handler
#define xPortPendSVHandler                     PendSV_Handler
#define xPortSysTickHandler                    SysTick_Handler

#define configASSERT(x)                        \
    do                                         \
    {                                          \
        if ((x) == 0)                          \
        {                                      \
            portDISABLE_INTERRUPTS();          \
            for (;;)                           \
            {                                  \
            }                                  \
        }                                      \
    } while (0)

#endif /* FREERTOS_CONFIG_H */