#ifndef __APP_CAN_H__
#define __APP_CAN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "can.h"

/* 电脑发送命令时使用的标准 CAN ID。 */
#define APP_CAN_COMMAND_ID  0x321U

/* STM32 返回响应时使用的标准 CAN ID。 */
#define APP_CAN_RESPONSE_ID 0x322U

/* 当前应用协议版本，后续修改报文格式时可以递增。 */
#define APP_CAN_PROTOCOL_VERSION 0x01U

/* Byte0 中支持的命令码。 */
#define APP_CAN_CMD_PING           0x01U
#define APP_CAN_CMD_GET_STATUS     0x02U
#define APP_CAN_CMD_CLEAR_COUNTERS 0x03U
#define APP_CAN_CMD_GET_DIAGNOSTICS 0x04U
#define APP_CAN_CMD_GET_ANALOG      0x05U
#define APP_CAN_CMD_SET_OUTPUT      0x06U
#define APP_CAN_CMD_GET_OUTPUT      0x07U
#define APP_CAN_CMD_RS485_SEND      0x08U
#define APP_CAN_CMD_RS485_STATUS    0x09U

/* Byte2 中返回的执行结果。 */
#define APP_CAN_RESULT_OK                  0x00U
#define APP_CAN_RESULT_UNSUPPORTED_COMMAND 0x01U
#define APP_CAN_RESULT_INVALID_LENGTH      0x02U
#define APP_CAN_RESULT_NOT_READY           0x03U
#define APP_CAN_RESULT_INVALID_PARAM       0x04U

/* 收到多少帧 ID=0x321 的有效命令。 */
extern volatile uint32_t app_can_command_count;

/* 成功放入发送邮箱的 0x322 回复数量。 */
extern volatile uint32_t app_can_response_count;

/* 回复失败的次数。 */
extern volatile uint32_t app_can_response_error_count;

/* 收到不支持的命令次数。 */
extern volatile uint32_t app_can_unsupported_command_count;

/* 收到长度不符合协议的命令次数。 */
extern volatile uint32_t app_can_invalid_length_count;

/* 成功执行的输出控制命令次数。 */
extern volatile uint32_t app_can_output_command_count;

/* 因通道号非法被拒绝的输出控制次数。 */
extern volatile uint32_t app_can_output_reject_count;

/* 成功发送 RS485 测试数据的命令次数。 */
extern volatile uint32_t app_can_rs485_send_count;

/* 处理 CAN 服务任务从 FreeRTOS 队列取出的一帧报文。 */
void APP_CAN_Process_Message(const CAN_ReceivedMessageTypeDef *message);

#ifdef __cplusplus
}
#endif

#endif /* __APP_CAN_H__ */
