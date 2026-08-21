#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* Cortex-M3 内核时钟是 72 MHz，FreeRTOS 用它产生 1 ms 系统节拍。 */
#define configCPU_CLOCK_HZ                      72000000UL
#define configTICK_RATE_HZ                      1000U

/* 开启抢占调度：高优先级任务就绪后可以立即运行。 */
#define configUSE_PREEMPTION                    1
#define configUSE_TIME_SLICING                  1
#define configUSE_TICKLESS_IDLE                 0

/* 当前工程只需要少量优先级，数值越大表示任务优先级越高。 */
#define configMAX_PRIORITIES                    5
#define configMINIMAL_STACK_SIZE                128U
#define configMAX_TASK_NAME_LEN                 16
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1

/* STM32F103C8 只有 20 KB RAM，先给 FreeRTOS 分配 6 KB 动态内存。 */
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configSUPPORT_STATIC_ALLOCATION         0
#define configTOTAL_HEAP_SIZE                   (6U * 1024U)
#define configAPPLICATION_ALLOCATED_HEAP        0

/* 本阶段使用轻量级任务通知，不启用尚未使用的软件定时器和信号量。 */
#define configUSE_TASK_NOTIFICATIONS            1
#define configUSE_MUTEXES                       0
#define configUSE_RECURSIVE_MUTEXES             0
#define configUSE_COUNTING_SEMAPHORES           0
#define configUSE_TIMERS                        0
#define configUSE_CO_ROUTINES                   0
#define configMAX_CO_ROUTINE_PRIORITIES         1

/* 打开内存申请失败和任务栈溢出检查，故障时停机便于调试。 */
#define configUSE_MALLOC_FAILED_HOOK            1
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0

#define configUSE_TRACE_FACILITY                0
#define configUSE_STATS_FORMATTING_FUNCTIONS    0
#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_NEWLIB_REENTRANT               0

/* STM32F103 实现了 4 个中断优先级位，合法优先级为 0~15。 */
#define configPRIO_BITS                         4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5
/*
 * ARMCC5 的内嵌汇编要求这里是直接立即数，不能使用 C 位移表达式。
 * 0xF0 对应优先级 15，0x50 对应优先级 5。
 */
#define configKERNEL_INTERRUPT_PRIORITY         0xF0
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    0x50

/* FreeRTOS 直接接管 SVC 和 PendSV，SysTick 由工程中断函数统一分发。 */
#define vPortSVCHandler                         SVC_Handler
#define xPortPendSVHandler                      PendSV_Handler

/* 只开放当前任务代码实际使用的可选 API。 */
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_vTaskDelete                     0
#define INCLUDE_vTaskSuspend                    0
#define INCLUDE_uxTaskPriorityGet               0
#define INCLUDE_vTaskPrioritySet                0
#define INCLUDE_xTaskGetIdleTaskHandle          0

/* 断言失败后关闭中断并停在原地，可在 Keil 调试器中查看调用栈。 */
#define configASSERT(condition)                 \
  if ((condition) == 0)                         \
  {                                             \
    taskDISABLE_INTERRUPTS();                   \
    for (;;)                                    \
    {                                           \
    }                                           \
  }

#endif /* FREERTOS_CONFIG_H */
