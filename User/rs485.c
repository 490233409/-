#include "rs485.h"

#include "queue.h"

UART_HandleTypeDef huart2;

static uint8_t rs485_rx_byte = 0U;

static volatile uint32_t rs485_rx_count = 0U;
static volatile uint32_t rs485_tx_count = 0U;
static volatile uint32_t rs485_error_count = 0U;
static volatile uint32_t rs485_last_error = HAL_UART_ERROR_NONE;
static volatile uint8_t rs485_last_rx_byte = 0U;

/* 中断往这里攒字节，总线空闲了整帧丢给队列 */
static RS485_FrameTypeDef rs485_rx_frame;

static uint8_t rs485_rx_frame_too_long = 0U;

static QueueHandle_t rs485_frame_queue_handle = NULL;

static volatile uint32_t rs485_rx_frame_count = 0U;
static volatile uint32_t rs485_frame_queue_overflow_count = 0U;
static volatile uint32_t rs485_frame_too_long_count = 0U;

/* DIR低电平接收 */
static void RS485_Set_Receive_Mode(void)
{
  HAL_GPIO_WritePin(RS485_DIR_GPIO_Port, RS485_DIR_Pin, GPIO_PIN_RESET);
}

/* DIR高电平发送 */
static void RS485_Set_Transmit_Mode(void)
{
  HAL_GPIO_WritePin(RS485_DIR_GPIO_Port, RS485_DIR_Pin, GPIO_PIN_SET);
}

void MX_RS485_UART_Init(void)
{
  GPIO_InitTypeDef gpio_init = {0};

  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* 上电先处于接收，别占着总线 */
  HAL_GPIO_WritePin(RS485_DIR_GPIO_Port, RS485_DIR_Pin, GPIO_PIN_RESET);
  gpio_init.Pin = RS485_DIR_Pin;
  gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
  gpio_init.Pull = GPIO_NOPULL;
  gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(RS485_DIR_GPIO_Port, &gpio_init);

  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;

  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }

  /* 一次收一个字节 */
  if (HAL_UART_Receive_IT(&huart2, &rs485_rx_byte, 1U) != HAL_OK)
  {
    Error_Handler();
  }

  /*
   * 开IDLE中断。Modbus RTU靠帧间静默划帧边界，
   * 115200下IDLE约87us就能发现空闲，比规范的t3.5（1.75ms）还早，
   * 对遵守规范的主机是安全的。
   */
  __HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);
}

BaseType_t RS485_FrameQueue_Init(void)
{
  if (rs485_frame_queue_handle != NULL)
  {
    return pdPASS;
  }

  rs485_frame_queue_handle = xQueueCreate(RS485_FRAME_QUEUE_SIZE,
                                          sizeof(RS485_FrameTypeDef));

  if (rs485_frame_queue_handle == NULL)
  {
    return pdFAIL;
  }

  return pdPASS;
}

/*
 * 发送。阻塞发完才切回接收，HAL_UART_Transmit返回时
 * 最后一个停止位还没离线就切方向的话会截尾。
 */
HAL_StatusTypeDef RS485_Send(const uint8_t *data, uint16_t length)
{
  HAL_StatusTypeDef status;

  if ((data == NULL) && (length > 0U))
  {
    return HAL_ERROR;
  }

  if (length == 0U)
  {
    return HAL_OK;
  }

  RS485_Set_Transmit_Mode();

  status = HAL_UART_Transmit(&huart2, (uint8_t *)data, length, 100U);

  RS485_Set_Receive_Mode();

  if (status == HAL_OK)
  {
    rs485_tx_count++;
  }
  else
  {
    rs485_error_count++;
    rs485_last_error = HAL_UART_GetError(&huart2);
  }

  return status;
}

void RS485_Get_Status(RS485_StatusTypeDef *status)
{
  uint32_t interrupt_state;

  if (status == NULL)
  {
    return;
  }

  /* 关一下中断，不然读到的是好几个时刻拼起来的值 */
  interrupt_state = __get_PRIMASK();
  __disable_irq();

  status->rx_count = rs485_rx_count;
  status->tx_count = rs485_tx_count;
  status->error_count = rs485_error_count;
  status->last_error = rs485_last_error;
  status->last_rx_byte = rs485_last_rx_byte;
  status->rx_frame_count = rs485_rx_frame_count;
  status->frame_queue_overflow_count = rs485_frame_queue_overflow_count;
  status->frame_too_long_count = rs485_frame_too_long_count;

  if (interrupt_state == 0U)
  {
    __enable_irq();
  }
}

void RS485_Clear_Statistics(void)
{
  uint32_t interrupt_state;

  interrupt_state = __get_PRIMASK();
  __disable_irq();

  rs485_rx_count = 0U;
  rs485_tx_count = 0U;
  rs485_error_count = 0U;
  rs485_last_error = HAL_UART_ERROR_NONE;
  rs485_last_rx_byte = 0U;
  rs485_rx_frame_count = 0U;
  rs485_frame_queue_overflow_count = 0U;
  rs485_frame_too_long_count = 0U;

  if (interrupt_state == 0U)
  {
    __enable_irq();
  }
}

BaseType_t RS485_Wait_Frame(RS485_FrameTypeDef *frame, TickType_t wait_ticks)
{
  if ((frame == NULL) || (rs485_frame_queue_handle == NULL))
  {
    return pdFAIL;
  }

  return xQueueReceive(rs485_frame_queue_handle, frame, wait_ticks);
}

/* IDLE中断：一帧收完了，整帧入队，跟CAN那边一个思路 */
void RS485_IdleDetectedFromISR(void)
{
  BaseType_t higher_priority_task_woken = pdFALSE;

  if (rs485_rx_frame.length > 0U)
  {
    if (rs485_rx_frame_too_long != 0U)
    {
      /* 超过256字节的帧直接整帧丢 */
      rs485_frame_too_long_count++;
    }
    else if (rs485_frame_queue_handle == NULL)
    {
      rs485_frame_queue_overflow_count++;
    }
    else if (xQueueSendFromISR(rs485_frame_queue_handle,
                               &rs485_rx_frame,
                               &higher_priority_task_woken) == pdPASS)
    {
      rs485_rx_frame_count++;
    }
    else
    {
      /* 队列满丢新帧保旧帧 */
      rs485_frame_queue_overflow_count++;
    }
  }

  rs485_rx_frame.length = 0U;
  rs485_rx_frame_too_long = 0U;

  portYIELD_FROM_ISR(higher_priority_task_woken);
}

/* 收完一个字节：追加进帧缓冲然后马上挂起下一次接收 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *uart_handle)
{
  if (uart_handle->Instance != USART2)
  {
    return;
  }

  rs485_last_rx_byte = rs485_rx_byte;
  rs485_rx_count++;

  if (rs485_rx_frame.length < RS485_FRAME_MAX_SIZE)
  {
    rs485_rx_frame.data[rs485_rx_frame.length] = rs485_rx_byte;
    rs485_rx_frame.length++;
  }
  else
  {
    rs485_rx_frame_too_long = 1U;
  }

  if (HAL_UART_Receive_IT(&huart2, &rs485_rx_byte, 1U) != HAL_OK)
  {
    rs485_error_count++;
    rs485_last_error = HAL_UART_GetError(&huart2);
  }
}

/* 出错也要重启接收，不然一次噪声就永远收不到东西了 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart_handle)
{
  if (uart_handle->Instance != USART2)
  {
    return;
  }

  rs485_error_count++;
  rs485_last_error = HAL_UART_GetError(&huart2);

  (void)HAL_UART_Receive_IT(&huart2, &rs485_rx_byte, 1U);
}
