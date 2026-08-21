#include "app_freertos.h"

#include "task.h"
#include "app_acquisition.h"
#include "app_can.h"
#include "app_modbus.h"
#include "can.h"
#include "rs485.h"
#include "usart.h"
#include "watchdog.h"

/* 栈单位是字，CAN和Modbus要跑协议所以给大点 */
#define CAN_TASK_STACK_SIZE       256U
#define HEARTBEAT_TASK_STACK_SIZE 128U
#define ACQUISITION_TASK_STACK_SIZE 128U
#define MODBUS_TASK_STACK_SIZE    256U
#define WATCHDOG_TASK_STACK_SIZE  128U

#define CAN_TASK_PRIORITY         (tskIDLE_PRIORITY + 2U)
#define HEARTBEAT_TASK_PRIORITY   (tskIDLE_PRIORITY + 1U)
#define ACQUISITION_TASK_PRIORITY (tskIDLE_PRIORITY + 1U)
#define MODBUS_TASK_PRIORITY      (tskIDLE_PRIORITY + 1U)

/* 喂狗任务要和CAN同级，不然卡在同级死循环里就喂不上了 */
#define WATCHDOG_TASK_PRIORITY    (tskIDLE_PRIORITY + 2U)

/* LSI有误差，IWDG最坏0.67s左右就超时，100ms喂一次留够余量 */
#define WATCHDOG_FEED_PERIOD_MS   100U

static TaskHandle_t can_task_handle = NULL;

static volatile uint32_t can_tx_success_count = 0U;
static volatile uint32_t can_tx_fail_count = 0U;

static void CAN_Service_Task(void *argument);
static void Heartbeat_Task(void *argument);
static void Acquisition_Task(void *argument);
static void Modbus_Task(void *argument);
static void Watchdog_Task(void *argument);

BaseType_t APP_FreeRTOS_Init(void)
{
  BaseType_t result;

  /* 队列要先建好再开中断，不然第一帧报文就丢了 */
  if (CAN_RxQueue_Init() != pdPASS)
  {
    return pdFAIL;
  }

  if (RS485_FrameQueue_Init() != pdPASS)
  {
    return pdFAIL;
  }

  result = xTaskCreate(CAN_Service_Task,
                       "CAN_Service",
                       CAN_TASK_STACK_SIZE,
                       NULL,
                       CAN_TASK_PRIORITY,
                       &can_task_handle);
  if (result != pdPASS)
  {
    return pdFAIL;
  }

  result = xTaskCreate(Heartbeat_Task,
                       "Heartbeat",
                       HEARTBEAT_TASK_STACK_SIZE,
                       NULL,
                       HEARTBEAT_TASK_PRIORITY,
                       NULL);
  if (result != pdPASS)
  {
    return pdFAIL;
  }

  result = xTaskCreate(Acquisition_Task,
                       "Acquisition",
                       ACQUISITION_TASK_STACK_SIZE,
                       NULL,
                       ACQUISITION_TASK_PRIORITY,
                       NULL);
  if (result != pdPASS)
  {
    return pdFAIL;
  }

  result = xTaskCreate(Modbus_Task,
                       "Modbus",
                       MODBUS_TASK_STACK_SIZE,
                       NULL,
                       MODBUS_TASK_PRIORITY,
                       NULL);
  if (result != pdPASS)
  {
    return pdFAIL;
  }

  result = xTaskCreate(Watchdog_Task,
                       "Watchdog",
                       WATCHDOG_TASK_STACK_SIZE,
                       NULL,
                       WATCHDOG_TASK_PRIORITY,
                       NULL);
  if (result != pdPASS)
  {
    return pdFAIL;
  }

  return pdPASS;
}

/* 只有这个任务碰CAN句柄，别的任务要发CAN都得走它 */
static void CAN_Service_Task(void *argument)
{
  CAN_ReceivedMessageTypeDef received_message = {0};
  uint32_t heartbeat_notifications;

  (void)argument;

  Debug_UART_Send_String("FreeRTOS scheduler running\r\n");

  for (;;)
  {
    Watchdog_Task_Checkin(WATCHDOG_TASK_CAN);

    if (CAN_Wait_Received_Message(&received_message,
                                  pdMS_TO_TICKS(10U)) == pdPASS)
    {
      APP_CAN_Process_Message(&received_message);
    }

    CAN_Process_Recovery();

    heartbeat_notifications = ulTaskNotifyTake(pdTRUE, 0U);
    if (heartbeat_notifications > 0U)
    {
      if (CAN_Send_Test_Message() == HAL_OK)
      {
        can_tx_success_count++;
        Debug_UART_Send_String("CAN TX queued: ID=0x123\r\n");
      }
      else
      {
        can_tx_fail_count++;
        Debug_UART_Send_String("CAN TX busy or failed\r\n");
      }
    }
  }
}

static void Heartbeat_Task(void *argument)
{
  TickType_t last_wake_tick;

  (void)argument;

  last_wake_tick = xTaskGetTickCount();

  for (;;)
  {
    /* 用DelayUntil而不是Delay，周期不会越跑越漂 */
    vTaskDelayUntil(&last_wake_tick, pdMS_TO_TICKS(1000U));

    xTaskNotifyGive(can_task_handle);

    Watchdog_Task_Checkin(WATCHDOG_TASK_HEARTBEAT);
  }
}

static void Acquisition_Task(void *argument)
{
  TickType_t last_wake_tick;

  (void)argument;
  last_wake_tick = xTaskGetTickCount();

  for (;;)
  {
    APP_Acquisition_Sample();

    Watchdog_Task_Checkin(WATCHDOG_TASK_ACQUISITION);

    vTaskDelayUntil(&last_wake_tick, pdMS_TO_TICKS(100U));
  }
}

static void Modbus_Task(void *argument)
{
  RS485_FrameTypeDef frame;

  (void)argument;

  for (;;)
  {
    Watchdog_Task_Checkin(WATCHDOG_TASK_MODBUS);

    /* 没帧时阻塞，500ms超时醒一次纯粹是为了喂狗报到 */
    if (RS485_Wait_Frame(&frame, pdMS_TO_TICKS(500U)) == pdPASS)
    {
      APP_Modbus_Process_Frame(&frame);
    }
  }
}

static void Watchdog_Task(void *argument)
{
  TickType_t last_wake_tick;

  (void)argument;

  last_wake_tick = xTaskGetTickCount();

  for (;;)
  {
    Watchdog_Feed_If_Healthy();

    vTaskDelayUntil(&last_wake_tick, pdMS_TO_TICKS(WATCHDOG_FEED_PERIOD_MS));
  }
}

void vApplicationMallocFailedHook(void)
{
  taskDISABLE_INTERRUPTS();
  for (;;)
  {
  }
}

void vApplicationStackOverflowHook(TaskHandle_t task_handle, char *task_name)
{
  (void)task_handle;
  (void)task_name;

  taskDISABLE_INTERRUPTS();
  for (;;)
  {
  }
}
