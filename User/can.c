/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    can.c
  * @brief   This file provides code for the configuration
  *          of the CAN instances.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "can.h"
#include "usart.h"
#include "queue.h"

/* Bus-Off先等硬件自动恢复1秒，不行再软件重启兜底 */
#define CAN_BUS_OFF_RESTART_DELAY_MS 1000U

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

CAN_HandleTypeDef hcan;

/* 16帧深度的接收队列 */
static QueueHandle_t can_rx_queue_handle = NULL;

/* 中断里更新的计数都标volatile */
volatile uint32_t can_rx_count = 0U;
volatile uint32_t can_rx_error_count = 0U;
volatile uint32_t can_rx_queue_overflow_count = 0U;
volatile uint32_t can_error_interrupt_count = 0U;
volatile uint32_t can_bus_off_count = 0U;
volatile uint32_t can_bus_off_recovery_count = 0U;
volatile uint32_t can_bus_off_recovery_failure_count = 0U;
volatile uint32_t can_last_error_code = HAL_CAN_ERROR_NONE;

/* 中断置1，任务里做完恢复清零 */
static volatile uint8_t can_bus_off_recovery_pending = 0U;

static volatile uint32_t can_bus_off_detect_tick = 0U;

static uint32_t can_bus_off_last_attempt_tick = 0U;

BaseType_t CAN_RxQueue_Init(void)
{
  if (can_rx_queue_handle != NULL)
  {
    return pdPASS;
  }

  can_rx_queue_handle = xQueueCreate(CAN_RX_QUEUE_SIZE,
                                     sizeof(CAN_ReceivedMessageTypeDef));

  if (can_rx_queue_handle == NULL)
  {
    return pdFAIL;
  }

  return pdPASS;
}

/* CAN init function */
void MX_CAN_Init(void)
{

  /* USER CODE BEGIN CAN_Init 0 */

  /* USER CODE END CAN_Init 0 */

  /* USER CODE BEGIN CAN_Init 1 */

  /* USER CODE END CAN_Init 1 */
  hcan.Instance = CAN1;
  hcan.Init.Prescaler = 4;
  hcan.Init.Mode = CAN_MODE_NORMAL;
  hcan.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan.Init.TimeSeg1 = CAN_BS1_5TQ;
  hcan.Init.TimeSeg2 = CAN_BS2_3TQ;
  hcan.Init.TimeTriggeredMode = DISABLE;
  hcan.Init.AutoBusOff = ENABLE;
  hcan.Init.AutoWakeUp = ENABLE;
  hcan.Init.AutoRetransmission = ENABLE;
  hcan.Init.ReceiveFifoLocked = DISABLE;
  hcan.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan) != HAL_OK)
  {
    Error_Handler();
  }

  if (CAN_Filter_Config() != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN_Init 2 */

  /* USER CODE END CAN_Init 2 */

}

void HAL_CAN_MspInit(CAN_HandleTypeDef* canHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(canHandle->Instance==CAN1)
  {
  /* USER CODE BEGIN CAN1_MspInit 0 */

  /* USER CODE END CAN1_MspInit 0 */
    /* CAN1 clock enable */
    __HAL_RCC_CAN1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**CAN GPIO Configuration
    PA11     ------> CAN_RX
    PA12     ------> CAN_TX
    */
    GPIO_InitStruct.Pin = CAN_RX_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(CAN_RX_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = CAN_TX_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(CAN_TX_GPIO_Port, &GPIO_InitStruct);

    /* CAN1 interrupt Init */
    /*
     * FreeRTOS要求调FromISR API的中断优先级数值大于等于5，
     * 所以这里不能设成0~4。
     */
    HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);

    HAL_NVIC_SetPriority(CAN1_SCE_IRQn, 6, 0);
    HAL_NVIC_EnableIRQ(CAN1_SCE_IRQn);
  /* USER CODE BEGIN CAN1_MspInit 1 */

  /* USER CODE END CAN1_MspInit 1 */
  }
}

void HAL_CAN_MspDeInit(CAN_HandleTypeDef* canHandle)
{

  if(canHandle->Instance==CAN1)
  {
  /* USER CODE BEGIN CAN1_MspDeInit 0 */

  /* USER CODE END CAN1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_CAN1_CLK_DISABLE();

    /**CAN GPIO Configuration
    PA11     ------> CAN_RX
    PA12     ------> CAN_TX
    */
    HAL_GPIO_DeInit(GPIOA, CAN_RX_Pin|CAN_TX_Pin);

    /* CAN1 interrupt Deinit */
    HAL_NVIC_DisableIRQ(USB_LP_CAN1_RX0_IRQn);
    HAL_NVIC_DisableIRQ(CAN1_SCE_IRQn);
  /* USER CODE BEGIN CAN1_MspDeInit 1 */

  /* USER CODE END CAN1_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

/* 全收，联调方便，以后再收窄到0x123/0x321 */
HAL_StatusTypeDef CAN_Filter_Config(void)
{
  CAN_FilterTypeDef filter_config = {0};

  filter_config.FilterBank = 0;
  filter_config.FilterMode = CAN_FILTERMODE_IDMASK;
  filter_config.FilterScale = CAN_FILTERSCALE_32BIT;
  filter_config.FilterFIFOAssignment = CAN_FILTER_FIFO0;
  filter_config.FilterActivation = CAN_FILTER_ENABLE;

  /* Mask全0表示所有位都不比较，什么ID都能过 */
  filter_config.FilterIdHigh = 0x0000;
  filter_config.FilterIdLow = 0x0000;
  filter_config.FilterMaskIdHigh = 0x0000;
  filter_config.FilterMaskIdLow = 0x0000;

  filter_config.SlaveStartFilterBank = 14;

  return HAL_CAN_ConfigFilter(&hcan, &filter_config);
}

HAL_StatusTypeDef CAN_Start(void)
{
  HAL_StatusTypeDef status;

  status = HAL_CAN_Start(&hcan);

  if (status != HAL_OK)
  {
    return status;
  }

  /* 开FIFO0接收通知和错误中断（含Bus-Off） */
  return HAL_CAN_ActivateNotification(&hcan,
                                      CAN_IT_RX_FIFO0_MSG_PENDING |
                                      CAN_IT_ERROR_WARNING |
                                      CAN_IT_ERROR_PASSIVE |
                                      CAN_IT_BUSOFF |
                                      CAN_IT_ERROR);
}

HAL_StatusTypeDef CAN_Send_Test_Message(void)
{
  uint8_t tx_data[8] = {
    0x11, 0x22, 0x33, 0x44,
    0x55, 0x66, 0x77, 0x88
  };

  return CAN_Send_Standard_Message(0x123U, tx_data, 8U);
}

HAL_StatusTypeDef CAN_Send_Standard_Message(uint16_t standard_id,
                                            const uint8_t *data,
                                            uint8_t length)
{
  CAN_TxHeaderTypeDef tx_header = {0};
  uint8_t tx_data[8] = {0};
  uint32_t tx_mailbox = 0;
  uint8_t index;

  if (standard_id > 0x7FFU)
  {
    return HAL_ERROR;
  }

  if (length > 8U)
  {
    return HAL_ERROR;
  }

  if ((length > 0U) && (data == NULL))
  {
    return HAL_ERROR;
  }

  for (index = 0U; index < length; index++)
  {
    tx_data[index] = data[index];
  }

  tx_header.StdId = standard_id;
  tx_header.ExtId = 0x00000000;
  tx_header.IDE = CAN_ID_STD;
  tx_header.RTR = CAN_RTR_DATA;
  tx_header.DLC = length;
  tx_header.TransmitGlobalTime = DISABLE;

  /* 3个邮箱都满就返回BUSY，别覆盖还没发出去的帧 */
  if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0U)
  {
    return HAL_BUSY;
  }

  return HAL_CAN_AddTxMessage(&hcan, &tx_header, tx_data, &tx_mailbox);
}

BaseType_t CAN_Wait_Received_Message(CAN_ReceivedMessageTypeDef *message,
                                     TickType_t wait_ticks)
{
  if ((message == NULL) || (can_rx_queue_handle == NULL))
  {
    return pdFAIL;
  }

  return xQueueReceive(can_rx_queue_handle, message, wait_ticks);
}

/*
 * Bus-Off恢复：先等ABOM硬件自动恢复，1秒还没好就Stop/Start重启一次。
 * 这些操作放任务里做，中断只负责置标志。
 */
void CAN_Process_Recovery(void)
{
  uint32_t current_tick;
  HAL_StatusTypeDef status;

  if (can_bus_off_recovery_pending == 0U)
  {
    return;
  }

  /* BOF清了说明硬件自己恢复了 */
  if (__HAL_CAN_GET_FLAG(&hcan, CAN_FLAG_BOF) == RESET)
  {
    can_bus_off_recovery_pending = 0U;
    can_bus_off_recovery_count++;
    HAL_CAN_ResetError(&hcan);
    Debug_UART_Send_String("CAN Bus-Off recovered automatically\r\n");
    return;
  }

  current_tick = HAL_GetTick();

  if ((current_tick - can_bus_off_detect_tick) <
      CAN_BUS_OFF_RESTART_DELAY_MS)
  {
    return;
  }

  /* 软件重启失败也隔1秒再试，别反复重启外设 */
  if ((current_tick - can_bus_off_last_attempt_tick) <
      CAN_BUS_OFF_RESTART_DELAY_MS)
  {
    return;
  }

  can_bus_off_last_attempt_tick = current_tick;

  status = HAL_CAN_Stop(&hcan);
  if (status == HAL_OK)
  {
    status = CAN_Start();
  }

  if (status == HAL_OK)
  {
    can_bus_off_recovery_pending = 0U;
    can_bus_off_recovery_count++;
    HAL_CAN_ResetError(&hcan);
    Debug_UART_Send_String("CAN Bus-Off recovered by restart\r\n");
  }
  else
  {
    can_bus_off_recovery_failure_count++;
    Debug_UART_Send_String("CAN Bus-Off recovery failed\r\n");
  }
}

/* 中断回调：只把报文塞队列，解析的事留给任务 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *can_handle)
{
  CAN_ReceivedMessageTypeDef received_message = {0};
  BaseType_t higher_priority_task_woken = pdFALSE;

  if (can_handle->Instance != CAN1)
  {
    return;
  }

  if (HAL_CAN_GetRxMessage(can_handle,
                           CAN_RX_FIFO0,
                           &received_message.header,
                           received_message.data) == HAL_OK)
  {
    can_rx_count++;

    if (can_rx_queue_handle == NULL)
    {
      can_rx_error_count++;
      return;
    }

    if (xQueueSendFromISR(can_rx_queue_handle,
                          &received_message,
                          &higher_priority_task_woken) != pdPASS)
    {
      /* 队列满了丢新帧保旧帧，计个数方便查 */
      can_rx_queue_overflow_count++;
    }

    portYIELD_FROM_ISR(higher_priority_task_woken);
  }
  else
  {
    can_rx_error_count++;
  }
}

void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *can_handle)
{
  uint32_t error_code;

  if (can_handle->Instance != CAN1)
  {
    return;
  }

  error_code = HAL_CAN_GetError(can_handle);
  can_last_error_code = error_code;
  can_error_interrupt_count++;

  /* 同一次Bus-Off可能触发多个错误标志，只记一次 */
  if (((error_code & HAL_CAN_ERROR_BOF) != 0U) &&
      (can_bus_off_recovery_pending == 0U))
  {
    can_bus_off_count++;
    can_bus_off_recovery_pending = 1U;
    can_bus_off_detect_tick = HAL_GetTick();
    can_bus_off_last_attempt_tick = can_bus_off_detect_tick;
  }
}

/* USER CODE END 1 */
