#include "app_modbus.h"

#include "app_acquisition.h"
#include "gpio_output.h"
#include "usart.h"
#include "watchdog.h"

volatile uint32_t app_modbus_request_count = 0U;
volatile uint32_t app_modbus_response_count = 0U;
volatile uint32_t app_modbus_crc_error_count = 0U;
volatile uint32_t app_modbus_exception_count = 0U;

/* 地址+功能码+CRC，最短4字节 */
#define APP_MODBUS_MIN_FRAME_SIZE 4U

/* 0x01/0x03/0x04/0x05/0x06的请求帧都是8字节 */
#define APP_MODBUS_FIXED_REQUEST_SIZE 8U

/*
 * CRC16，多项式0xA001初值0xFFFF，按位算的。
 * 查表法要占512字节Flash，帧这么短没必要。
 */
static uint16_t APP_Modbus_CRC16(const uint8_t *data, uint16_t length)
{
  uint16_t crc = 0xFFFFU;
  uint16_t byte_index;
  uint8_t bit_index;

  for (byte_index = 0U; byte_index < length; byte_index++)
  {
    crc = (uint16_t)(crc ^ data[byte_index]);

    for (bit_index = 0U; bit_index < 8U; bit_index++)
    {
      if ((crc & 0x0001U) != 0U)
      {
        crc = (uint16_t)((crc >> 1U) ^ 0xA001U);
      }
      else
      {
        crc = (uint16_t)(crc >> 1U);
      }
    }
  }

  return crc;
}

/* 组帧发送：地址+功能码+数据+CRC，CRC低字节在前 */
static void APP_Modbus_Send_Response(uint8_t function_code,
                                     const uint8_t *body,
                                     uint16_t body_length)
{
  /* 最大的响应是0x03读8个寄存器，21字节，32够用 */
  uint8_t tx_frame[32U];
  uint16_t tx_length = 0U;
  uint16_t crc;
  uint16_t index;

  if ((uint32_t)body_length + 4U > (uint32_t)sizeof(tx_frame))
  {
    return;
  }

  tx_frame[tx_length] = APP_MODBUS_SLAVE_ADDRESS;
  tx_length++;
  tx_frame[tx_length] = function_code;
  tx_length++;

  for (index = 0U; index < body_length; index++)
  {
    tx_frame[tx_length] = body[index];
    tx_length++;
  }

  crc = APP_Modbus_CRC16(tx_frame, tx_length);
  tx_frame[tx_length] = (uint8_t)(crc & 0xFFU);
  tx_length++;
  tx_frame[tx_length] = (uint8_t)((crc >> 8U) & 0xFFU);
  tx_length++;

  if (RS485_Send(tx_frame, tx_length) == HAL_OK)
  {
    app_modbus_response_count++;
  }
}

/* 异常响应：功能码|0x80，跟着一个异常码 */
static void APP_Modbus_Send_Exception(uint8_t function_code,
                                      uint8_t exception_code)
{
  uint8_t body[1];

  body[0] = exception_code;
  app_modbus_exception_count++;
  APP_Modbus_Send_Response((uint8_t)(function_code | 0x80U), body, 1U);
}

/* Modbus多字节都是大端，和STM32相反 */
static uint16_t APP_Modbus_Read_BE16(const uint8_t *data)
{
  return (uint16_t)(((uint16_t)data[0] << 8U) | (uint16_t)data[1]);
}

/* 0x01 读线圈，返回PB0/PB1状态，bit0对应起始地址 */
static void APP_Modbus_Handle_Read_Coils(const RS485_FrameTypeDef *frame)
{
  uint16_t start_address;
  uint16_t quantity;
  uint8_t body[2];
  uint8_t coil_bitmap = 0U;
  uint16_t index;

  if (frame->length != APP_MODBUS_FIXED_REQUEST_SIZE)
  {
    APP_Modbus_Send_Exception(APP_MODBUS_FC_READ_COILS,
                              APP_MODBUS_EX_ILLEGAL_DATA_VALUE);
    return;
  }

  start_address = APP_Modbus_Read_BE16(&frame->data[2]);
  quantity = APP_Modbus_Read_BE16(&frame->data[4]);

  if ((quantity == 0U) || (quantity > APP_MODBUS_COIL_COUNT))
  {
    APP_Modbus_Send_Exception(APP_MODBUS_FC_READ_COILS,
                              APP_MODBUS_EX_ILLEGAL_DATA_VALUE);
    return;
  }

  if ((start_address + quantity) > APP_MODBUS_COIL_COUNT)
  {
    APP_Modbus_Send_Exception(APP_MODBUS_FC_READ_COILS,
                              APP_MODBUS_EX_ILLEGAL_DATA_ADDRESS);
    return;
  }

  for (index = 0U; index < quantity; index++)
  {
    if (GPIO_Output_Get((uint8_t)(start_address + index)) != 0U)
    {
      coil_bitmap = (uint8_t)(coil_bitmap | (uint8_t)(1U << index));
    }
  }

  body[0] = 1U;
  body[1] = coil_bitmap;
  APP_Modbus_Send_Response(APP_MODBUS_FC_READ_COILS, body, 2U);
}

/* 0x04 读输入寄存器：0=ADC原始值 1=毫伏 2=有效标志 */
static void APP_Modbus_Handle_Read_Input_Regs(const RS485_FrameTypeDef *frame)
{
  APP_AcquisitionDataTypeDef acquisition_data = {0};
  uint16_t input_registers[APP_MODBUS_INPUT_COUNT];
  uint16_t start_address;
  uint16_t quantity;
  uint8_t body[1U + (APP_MODBUS_INPUT_COUNT * 2U)];
  uint16_t body_length;
  uint16_t index;
  uint16_t value;

  if (frame->length != APP_MODBUS_FIXED_REQUEST_SIZE)
  {
    APP_Modbus_Send_Exception(APP_MODBUS_FC_READ_INPUT_REGS,
                              APP_MODBUS_EX_ILLEGAL_DATA_VALUE);
    return;
  }

  start_address = APP_Modbus_Read_BE16(&frame->data[2]);
  quantity = APP_Modbus_Read_BE16(&frame->data[4]);

  if ((quantity == 0U) || (quantity > APP_MODBUS_INPUT_COUNT))
  {
    APP_Modbus_Send_Exception(APP_MODBUS_FC_READ_INPUT_REGS,
                              APP_MODBUS_EX_ILLEGAL_DATA_VALUE);
    return;
  }

  if ((start_address + quantity) > APP_MODBUS_INPUT_COUNT)
  {
    APP_Modbus_Send_Exception(APP_MODBUS_FC_READ_INPUT_REGS,
                              APP_MODBUS_EX_ILLEGAL_DATA_ADDRESS);
    return;
  }

  /* 一次拿整个快照，三个寄存器才是同一次采样的值 */
  APP_Acquisition_Get_Latest(&acquisition_data);
  input_registers[APP_MODBUS_INPUT_ADDR_ADC_RAW] = acquisition_data.raw_value;
  input_registers[APP_MODBUS_INPUT_ADDR_ADC_MV] = acquisition_data.millivolts;
  input_registers[APP_MODBUS_INPUT_ADDR_VALID] = acquisition_data.valid;

  body[0] = (uint8_t)(quantity * 2U);
  body_length = 1U;
  for (index = 0U; index < quantity; index++)
  {
    value = input_registers[start_address + index];
    body[body_length] = (uint8_t)((value >> 8U) & 0xFFU);
    body_length++;
    body[body_length] = (uint8_t)(value & 0xFFU);
    body_length++;
  }

  APP_Modbus_Send_Response(APP_MODBUS_FC_READ_INPUT_REGS, body, body_length);
}

/* 0x03 读保持寄存器：从站信息、统计、看门狗状态 */
static void APP_Modbus_Handle_Read_Holding_Regs(const RS485_FrameTypeDef *frame)
{
  uint16_t holding_registers[APP_MODBUS_HOLDING_COUNT];
  uint16_t start_address;
  uint16_t quantity;
  uint8_t body[1U + (APP_MODBUS_HOLDING_COUNT * 2U)];
  uint16_t body_length;
  uint16_t index;
  uint16_t value;

  if (frame->length != APP_MODBUS_FIXED_REQUEST_SIZE)
  {
    APP_Modbus_Send_Exception(APP_MODBUS_FC_READ_HOLDING_REGS,
                              APP_MODBUS_EX_ILLEGAL_DATA_VALUE);
    return;
  }

  start_address = APP_Modbus_Read_BE16(&frame->data[2]);
  quantity = APP_Modbus_Read_BE16(&frame->data[4]);

  if ((quantity == 0U) || (quantity > APP_MODBUS_HOLDING_COUNT))
  {
    APP_Modbus_Send_Exception(APP_MODBUS_FC_READ_HOLDING_REGS,
                              APP_MODBUS_EX_ILLEGAL_DATA_VALUE);
    return;
  }

  if ((start_address + quantity) > APP_MODBUS_HOLDING_COUNT)
  {
    APP_Modbus_Send_Exception(APP_MODBUS_FC_READ_HOLDING_REGS,
                              APP_MODBUS_EX_ILLEGAL_DATA_ADDRESS);
    return;
  }

  holding_registers[APP_MODBUS_HOLDING_ADDR_SLAVE_ID] = APP_MODBUS_SLAVE_ADDRESS;
  holding_registers[APP_MODBUS_HOLDING_ADDR_MAP_VERSION] = APP_MODBUS_MAP_VERSION;
  holding_registers[APP_MODBUS_HOLDING_ADDR_REQUESTS] =
    (uint16_t)(app_modbus_request_count & 0xFFFFU);
  holding_registers[APP_MODBUS_HOLDING_ADDR_CRC_ERRORS] =
    (uint16_t)(app_modbus_crc_error_count & 0xFFFFU);
  holding_registers[APP_MODBUS_HOLDING_ADDR_EXCEPTIONS] =
    (uint16_t)(app_modbus_exception_count & 0xFFFFU);
  holding_registers[APP_MODBUS_HOLDING_ADDR_RESPONSES] =
    (uint16_t)(app_modbus_response_count & 0xFFFFU);
  holding_registers[APP_MODBUS_HOLDING_ADDR_HEALTH] =
    (uint16_t)Watchdog_Get_Health_Bitmap();
  holding_registers[APP_MODBUS_HOLDING_ADDR_IWDG_RESETS] =
    Watchdog_Get_Iwdg_Reset_Count();

  body[0] = (uint8_t)(quantity * 2U);
  body_length = 1U;
  for (index = 0U; index < quantity; index++)
  {
    value = holding_registers[start_address + index];
    body[body_length] = (uint8_t)((value >> 8U) & 0xFFU);
    body_length++;
    body[body_length] = (uint8_t)(value & 0xFFU);
    body_length++;
  }

  APP_Modbus_Send_Response(APP_MODBUS_FC_READ_HOLDING_REGS, body, body_length);
}

/*
 * 0x05 写单线圈。广播时respond=0只执行不回复。
 * 写0xFF00闭合，0x0000断开，别的值协议规定要拒绝。
 */
static void APP_Modbus_Handle_Write_Single_Coil(const RS485_FrameTypeDef *frame,
                                                uint8_t respond)
{
  uint16_t coil_address;
  uint16_t coil_value;
  uint8_t target_state;

  if (frame->length != APP_MODBUS_FIXED_REQUEST_SIZE)
  {
    if (respond != 0U)
    {
      APP_Modbus_Send_Exception(APP_MODBUS_FC_WRITE_SINGLE_COIL,
                                APP_MODBUS_EX_ILLEGAL_DATA_VALUE);
    }
    return;
  }

  coil_address = APP_Modbus_Read_BE16(&frame->data[2]);
  coil_value = APP_Modbus_Read_BE16(&frame->data[4]);

  if (coil_address >= APP_MODBUS_COIL_COUNT)
  {
    if (respond != 0U)
    {
      APP_Modbus_Send_Exception(APP_MODBUS_FC_WRITE_SINGLE_COIL,
                                APP_MODBUS_EX_ILLEGAL_DATA_ADDRESS);
    }
    return;
  }

  if ((coil_value != APP_MODBUS_COIL_ON_VALUE) &&
      (coil_value != APP_MODBUS_COIL_OFF_VALUE))
  {
    if (respond != 0U)
    {
      APP_Modbus_Send_Exception(APP_MODBUS_FC_WRITE_SINGLE_COIL,
                                APP_MODBUS_EX_ILLEGAL_DATA_VALUE);
    }
    return;
  }

  target_state = (coil_value == APP_MODBUS_COIL_ON_VALUE) ? 1U : 0U;
  (void)GPIO_Output_Set((uint8_t)coil_address, target_state);

  /* 正常响应就是把请求的4字节数据原样回显 */
  if (respond != 0U)
  {
    APP_Modbus_Send_Response(APP_MODBUS_FC_WRITE_SINGLE_COIL,
                             &frame->data[2], 4U);
  }
}

/*
 * 0x06 写单寄存器，只开放地址0的命令寄存器。
 * 写0xDEAD触发看门狗卡死测试，先把回显发出去再死，
 * 不然主机收不到响应会误判。
 */
static void APP_Modbus_Handle_Write_Single_Reg(const RS485_FrameTypeDef *frame,
                                               uint8_t respond)
{
  uint16_t reg_address;
  uint16_t reg_value;

  if (frame->length != APP_MODBUS_FIXED_REQUEST_SIZE)
  {
    if (respond != 0U)
    {
      APP_Modbus_Send_Exception(APP_MODBUS_FC_WRITE_SINGLE_REG,
                                APP_MODBUS_EX_ILLEGAL_DATA_VALUE);
    }
    return;
  }

  reg_address = APP_Modbus_Read_BE16(&frame->data[2]);
  reg_value = APP_Modbus_Read_BE16(&frame->data[4]);

  if (reg_address != APP_MODBUS_CMD_REG_ADDRESS)
  {
    if (respond != 0U)
    {
      APP_Modbus_Send_Exception(APP_MODBUS_FC_WRITE_SINGLE_REG,
                                APP_MODBUS_EX_ILLEGAL_DATA_ADDRESS);
    }
    return;
  }

  if (reg_value != APP_MODBUS_CMD_WDOG_HANG_TEST)
  {
    if (respond != 0U)
    {
      APP_Modbus_Send_Exception(APP_MODBUS_FC_WRITE_SINGLE_REG,
                                APP_MODBUS_EX_ILLEGAL_DATA_VALUE);
    }
    return;
  }

  if (respond != 0U)
  {
    APP_Modbus_Send_Response(APP_MODBUS_FC_WRITE_SINGLE_REG,
                             &frame->data[2], 4U);
  }

  /* 回显发完了，死给看门狗看，函数不会返回 */
  Watchdog_Trigger_Hang_Test();
}

/*
 * 处理一帧请求。CRC错、地址不是本站、帧太短都按规范直接丢，
 * 不回任何东西。广播只执行写操作不回复。
 */
void APP_Modbus_Process_Frame(const RS485_FrameTypeDef *frame)
{
  uint8_t address;
  uint8_t function_code;
  uint8_t is_broadcast = 0U;
  uint16_t received_crc;
  uint16_t computed_crc;

  if (frame == NULL)
  {
    return;
  }

  if (frame->length < APP_MODBUS_MIN_FRAME_SIZE)
  {
    return;
  }

  address = frame->data[0];
  function_code = frame->data[1];

  if (address == 0U)
  {
    is_broadcast = 1U;
  }
  else if (address != APP_MODBUS_SLAVE_ADDRESS)
  {
    return;
  }

  /* CRC低字节在前 */
  received_crc = (uint16_t)((uint16_t)frame->data[frame->length - 2U] |
                 ((uint16_t)frame->data[frame->length - 1U] << 8U));
  computed_crc = APP_Modbus_CRC16(frame->data,
                                  (uint16_t)(frame->length - 2U));
  if (received_crc != computed_crc)
  {
    app_modbus_crc_error_count++;
    return;
  }

  app_modbus_request_count++;

  if (is_broadcast != 0U)
  {
    if (function_code == APP_MODBUS_FC_WRITE_SINGLE_COIL)
    {
      APP_Modbus_Handle_Write_Single_Coil(frame, 0U);
    }
    else if (function_code == APP_MODBUS_FC_WRITE_SINGLE_REG)
    {
      APP_Modbus_Handle_Write_Single_Reg(frame, 0U);
    }
    return;
  }

  switch (function_code)
  {
    case APP_MODBUS_FC_READ_COILS:
      APP_Modbus_Handle_Read_Coils(frame);
      break;

    case APP_MODBUS_FC_READ_HOLDING_REGS:
      APP_Modbus_Handle_Read_Holding_Regs(frame);
      break;

    case APP_MODBUS_FC_READ_INPUT_REGS:
      APP_Modbus_Handle_Read_Input_Regs(frame);
      break;

    case APP_MODBUS_FC_WRITE_SINGLE_COIL:
      APP_Modbus_Handle_Write_Single_Coil(frame, 1U);
      break;

    case APP_MODBUS_FC_WRITE_SINGLE_REG:
      APP_Modbus_Handle_Write_Single_Reg(frame, 1U);
      break;

    default:
      APP_Modbus_Send_Exception(function_code,
                                APP_MODBUS_EX_ILLEGAL_FUNCTION);
      break;
  }
}
