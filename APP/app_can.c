#include "app_can.h"
#include "app_acquisition.h"
#include "can.h"
#include "gpio_output.h"
#include "rs485.h"
#include "usart.h"

volatile uint32_t app_can_command_count = 0U;
volatile uint32_t app_can_response_count = 0U;
volatile uint32_t app_can_response_error_count = 0U;
volatile uint32_t app_can_unsupported_command_count = 0U;
volatile uint32_t app_can_invalid_length_count = 0U;
volatile uint32_t app_can_output_command_count = 0U;
volatile uint32_t app_can_output_reject_count = 0U;
volatile uint32_t app_can_rs485_send_count = 0U;

/* 命令帧0x321，响应帧0x322，Byte0命令码 Byte1序号，响应的Byte0要|0x80 */
void APP_CAN_Process_Message(const CAN_ReceivedMessageTypeDef *message)
{
  uint8_t response_data[8] = {0};
  uint8_t command = 0U;
  uint8_t sequence = 0U;
  uint32_t rx_count_snapshot;
  APP_AcquisitionDataTypeDef acquisition_data = {0};
  uint8_t output_channel = 0U;
  uint8_t output_state = 0U;

  if (message == NULL)
  {
    return;
  }

  /* 只处理标准数据帧 */
  if (message->header.IDE != CAN_ID_STD)
  {
    return;
  }

  if (message->header.RTR != CAN_RTR_DATA)
  {
    return;
  }

  if (message->header.StdId != APP_CAN_COMMAND_ID)
  {
    return;
  }

  app_can_command_count++;

  if (message->header.DLC > 0U)
  {
    command = message->data[0];
  }

  if (message->header.DLC > 1U)
  {
    sequence = message->data[1];
  }

  response_data[0] = command | 0x80U;
  response_data[1] = sequence;

  if (message->header.DLC < 2U)
  {
    /* 太短也要回错误码，不然上位机不知道发没发到 */
    response_data[2] = APP_CAN_RESULT_INVALID_LENGTH;
    app_can_invalid_length_count++;
    Debug_UART_Send_String("CAN command invalid length\r\n");
  }
  else
  {
    switch (command)
    {
      case APP_CAN_CMD_PING:
        response_data[2] = APP_CAN_RESULT_OK;
        response_data[3] = 'P';
        response_data[4] = 'O';
        response_data[5] = 'N';
        response_data[6] = 'G';
        response_data[7] = APP_CAN_PROTOCOL_VERSION;
        Debug_UART_Send_String("CAN command: PING\r\n");
        break;

      case APP_CAN_CMD_GET_STATUS:
        /* 先存下来再拆高低字节，不然后面读的时候值可能已经变了 */
        rx_count_snapshot = can_rx_count;

        response_data[2] = APP_CAN_RESULT_OK;
        response_data[3] = (uint8_t)HAL_CAN_GetState(&hcan);
        response_data[4] = (uint8_t)(HAL_CAN_GetError(&hcan) & 0xFFU);
        response_data[5] = (uint8_t)(rx_count_snapshot & 0xFFU);
        response_data[6] = (uint8_t)((rx_count_snapshot >> 8U) & 0xFFU);
        response_data[7] = (uint8_t)(can_rx_queue_overflow_count & 0xFFU);
        Debug_UART_Send_String("CAN command: GET_STATUS\r\n");
        break;

      case APP_CAN_CMD_CLEAR_COUNTERS:
        can_rx_count = 0U;
        can_rx_error_count = 0U;
        can_rx_queue_overflow_count = 0U;
        can_error_interrupt_count = 0U;
        can_bus_off_count = 0U;
        can_bus_off_recovery_count = 0U;
        can_bus_off_recovery_failure_count = 0U;
        can_last_error_code = HAL_CAN_ERROR_NONE;
        HAL_CAN_ResetError(&hcan);
        app_can_command_count = 0U;
        app_can_response_count = 0U;
        app_can_response_error_count = 0U;
        app_can_unsupported_command_count = 0U;
        app_can_invalid_length_count = 0U;
        app_can_output_command_count = 0U;
        app_can_output_reject_count = 0U;
        app_can_rs485_send_count = 0U;

        response_data[2] = APP_CAN_RESULT_OK;
        Debug_UART_Send_String("CAN command: CLEAR_COUNTERS\r\n");
        break;

      case APP_CAN_CMD_GET_DIAGNOSTICS:
        response_data[2] = APP_CAN_RESULT_OK;
        response_data[3] = (uint8_t)(can_bus_off_count & 0xFFU);
        response_data[4] =
          (uint8_t)(can_bus_off_recovery_count & 0xFFU);
        response_data[5] =
          (uint8_t)(can_bus_off_recovery_failure_count & 0xFFU);
        response_data[6] =
          (uint8_t)(can_error_interrupt_count & 0xFFU);
        response_data[7] = (uint8_t)(can_last_error_code & 0xFFU);
        Debug_UART_Send_String("CAN command: GET_DIAGNOSTICS\r\n");
        break;

      case APP_CAN_CMD_GET_ANALOG:
        APP_Acquisition_Get_Latest(&acquisition_data);

        if (acquisition_data.valid != 0U)
        {
          response_data[2] = APP_CAN_RESULT_OK;
        }
        else
        {
          response_data[2] = APP_CAN_RESULT_NOT_READY;
        }

        response_data[3] =
          (uint8_t)(acquisition_data.raw_value & 0xFFU);
        response_data[4] =
          (uint8_t)((acquisition_data.raw_value >> 8U) & 0xFFU);
        response_data[5] =
          (uint8_t)(acquisition_data.millivolts & 0xFFU);
        response_data[6] =
          (uint8_t)((acquisition_data.millivolts >> 8U) & 0xFFU);
        response_data[7] = acquisition_data.valid;
        Debug_UART_Send_String("CAN command: GET_ANALOG\r\n");
        break;

      case APP_CAN_CMD_SET_OUTPUT:
        /* Byte2通道 Byte3状态，至少4字节 */
        if (message->header.DLC < 4U)
        {
          response_data[2] = APP_CAN_RESULT_INVALID_LENGTH;
          app_can_invalid_length_count++;
          Debug_UART_Send_String("CAN SET_OUTPUT length error\r\n");
          break;
        }

        output_channel = message->data[2];
        output_state = message->data[3];

        if (GPIO_Output_Set(output_channel, output_state) == HAL_OK)
        {
          response_data[2] = APP_CAN_RESULT_OK;
          app_can_output_command_count++;
          Debug_UART_Send_String("CAN command: SET_OUTPUT\r\n");
        }
        else
        {
          /* 通道号只有0和1 */
          response_data[2] = APP_CAN_RESULT_INVALID_PARAM;
          app_can_output_reject_count++;
          Debug_UART_Send_String("CAN SET_OUTPUT invalid channel\r\n");
        }

        /* 把实际状态带回去 */
        response_data[3] = output_channel;
        response_data[4] = GPIO_Output_Get(output_channel);
        response_data[5] = GPIO_Output_Get_Bitmap();
        response_data[6] = GPIO_OUTPUT_CHANNEL_COUNT;
        break;

      case APP_CAN_CMD_GET_OUTPUT:
        response_data[2] = APP_CAN_RESULT_OK;
        response_data[3] = GPIO_Output_Get_Bitmap();
        response_data[4] = GPIO_Output_Get(0U);
        response_data[5] = GPIO_Output_Get(1U);
        response_data[6] = GPIO_OUTPUT_CHANNEL_COUNT;
        response_data[7] = (uint8_t)(app_can_output_command_count & 0xFFU);
        Debug_UART_Send_String("CAN command: GET_OUTPUT\r\n");
        break;

      case APP_CAN_CMD_RS485_SEND:
        /* Byte2长度(最多5)，Byte3开始是数据 */
        if ((message->header.DLC < 3U) || (message->data[2] > 5U) ||
            (message->header.DLC < (uint32_t)(3U + message->data[2])))
        {
          response_data[2] = APP_CAN_RESULT_INVALID_LENGTH;
          app_can_invalid_length_count++;
          Debug_UART_Send_String("CAN RS485_SEND length error\r\n");
          break;
        }

        if (RS485_Send(&message->data[3], message->data[2]) == HAL_OK)
        {
          response_data[2] = APP_CAN_RESULT_OK;
          response_data[3] = message->data[2];
          app_can_rs485_send_count++;
          Debug_UART_Send_String("CAN command: RS485_SEND\r\n");
        }
        else
        {
          response_data[2] = APP_CAN_RESULT_INVALID_PARAM;
          Debug_UART_Send_String("CAN RS485_SEND failed\r\n");
        }
        break;

      case APP_CAN_CMD_RS485_STATUS:
        {
          RS485_StatusTypeDef rs485_status = {0};

          RS485_Get_Status(&rs485_status);
          response_data[2] = APP_CAN_RESULT_OK;
          response_data[3] = (uint8_t)(rs485_status.rx_count & 0xFFU);
          response_data[4] = (uint8_t)(rs485_status.tx_count & 0xFFU);
          response_data[5] = (uint8_t)(rs485_status.error_count & 0xFFU);
          response_data[6] = rs485_status.last_rx_byte;
          response_data[7] = (uint8_t)(rs485_status.last_error & 0xFFU);
          Debug_UART_Send_String("CAN command: RS485_STATUS\r\n");
        }
        break;

      default:
        response_data[2] = APP_CAN_RESULT_UNSUPPORTED_COMMAND;
        app_can_unsupported_command_count++;
        Debug_UART_Send_String("CAN command unsupported\r\n");
        break;
    }
  }

  /* 固定回8字节 */
  if (CAN_Send_Standard_Message(APP_CAN_RESPONSE_ID,
                                response_data,
                                8U) == HAL_OK)
  {
    app_can_response_count++;
    Debug_UART_Send_String("CAN response queued: ID=0x322\r\n");
  }
  else
  {
    app_can_response_error_count++;
    Debug_UART_Send_String("CAN response failed\r\n");
  }
}
