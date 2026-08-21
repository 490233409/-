#include "watchdog.h"

#include "FreeRTOS.h"
#include "task.h"
#include "usart.h"

/*
 * LSI按40kHz算，32分频后1250Hz，重载1250就是1秒。
 * 但LSI实际会在30~60kHz漂，所以真实超时在0.67~1.33秒之间，
 * 喂狗周期必须比0.67秒短很多。
 */
#define WATCHDOG_TIMEOUT_RELOAD 1250U

typedef struct
{
  uint32_t last_checkin_tick;
  uint32_t deadline_ms;
  const char *name;
} Watchdog_TaskMonitorTypeDef;

IWDG_HandleTypeDef hiwdg;

/* 期限按各任务周期的2~3倍给，容忍抖动 */
static volatile Watchdog_TaskMonitorTypeDef task_monitors[WATCHDOG_TASK_COUNT] =
{
  { 0U, 200U,  "CAN_Service" },
  { 0U, 2500U, "Heartbeat"   },
  { 0U, 500U,  "Acquisition" },
  { 0U, 1500U, "Modbus"      }
};

static volatile uint32_t watchdog_feed_count = 0U;

/* 卡死只报一次，不然串口日志会被刷爆 */
static uint8_t stall_reported[WATCHDOG_TASK_COUNT] = {0U};

/*
 * 启动IWDG，顺便把复位原因记下来。
 * 狗复位次数存BKP->DR1，这个寄存器复位后不清零（断电才丢），
 * 所以能查到设备一共被狗救过几次。
 */
void MX_IWDG_Init(void)
{
  uint16_t reset_count;

  /* 先记复位原因再启动狗，狗一启动就停不下来了 */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_RCC_BKP_CLK_ENABLE();

  /* 要先置DBP位才允许写BKP */
  PWR->CR |= PWR_CR_DBP;

  if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET)
  {
    reset_count = (uint16_t)((BKP->DR1 & 0xFFFFU) + 1U);
    BKP->DR1 = reset_count;
    Debug_UART_Send_String("Reset cause: IWDG watchdog reset\r\n");
  }
  else
  {
    Debug_UART_Send_String("Reset cause: power-on or pin reset\r\n");
  }

  /* 复位标志要手动清，不然下次上电还会读出IWDG复位 */
  __HAL_RCC_CLEAR_RESET_FLAGS();

  hiwdg.Instance = IWDG;
  hiwdg.Init.Prescaler = IWDG_PRESCALER_32;
  hiwdg.Init.Reload = WATCHDOG_TIMEOUT_RELOAD;
  if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
  {
    Error_Handler();
  }

  /* 调试器暂停时狗也暂停，不然单步几秒就被复位了 */
  DBGMCU->CR |= DBGMCU_CR_DBG_IWDG_STOP;

  Debug_UART_Send_String("IWDG started, timeout about 1 second\r\n");
}

/* 32位写本身就是原子的，不用加锁 */
void Watchdog_Task_Checkin(uint8_t task_id)
{
  if (task_id >= WATCHDOG_TASK_COUNT)
  {
    return;
  }

  task_monitors[task_id].last_checkin_tick = xTaskGetTickCount();
}

/*
 * 所有任务都在期限内报到过才喂狗，有一个卡死就不喂，
 * 然后IWDG大概1秒后把板子复位。
 * 无符号减法在tick回绕的时候结果也是对的。
 */
void Watchdog_Feed_If_Healthy(void)
{
  TickType_t now;
  uint8_t index;
  uint8_t all_healthy = 1U;

  now = xTaskGetTickCount();

  for (index = 0U; index < WATCHDOG_TASK_COUNT; index++)
  {
    if ((uint32_t)(now - task_monitors[index].last_checkin_tick) <=
        pdMS_TO_TICKS(task_monitors[index].deadline_ms))
    {
      stall_reported[index] = 0U;
    }
    else
    {
      all_healthy = 0U;

      if (stall_reported[index] == 0U)
      {
        stall_reported[index] = 1U;
        Debug_UART_Send_String("Watchdog: task ");
        Debug_UART_Send_String(task_monitors[index].name);
        Debug_UART_Send_String(" stalled, stop feeding\r\n");
      }
    }
  }

  if (all_healthy != 0U)
  {
    if (HAL_IWDG_Refresh(&hiwdg) == HAL_OK)
    {
      watchdog_feed_count++;
    }
  }
}

/* bit为1表示对应任务活着，0x0F就是4个都在 */
uint8_t Watchdog_Get_Health_Bitmap(void)
{
  TickType_t now;
  uint8_t index;
  uint8_t bitmap = 0U;

  now = xTaskGetTickCount();

  for (index = 0U; index < WATCHDOG_TASK_COUNT; index++)
  {
    if ((uint32_t)(now - task_monitors[index].last_checkin_tick) <=
        pdMS_TO_TICKS(task_monitors[index].deadline_ms))
    {
      bitmap = (uint8_t)(bitmap | (uint8_t)(1U << index));
    }
  }

  return bitmap;
}

uint16_t Watchdog_Get_Iwdg_Reset_Count(void)
{
  return (uint16_t)(BKP->DR1 & 0xFFFFU);
}

/*
 * 测试用的：原地空转不报到，等狗来复位。
 * 不关中断，其他任务照常跑，这样才是"只有本任务卡死"的场景。
 */
void Watchdog_Trigger_Hang_Test(void)
{
  Debug_UART_Send_String("Watchdog hang test: this task stops checking in\r\n");

  for (;;)
  {
  }
}
