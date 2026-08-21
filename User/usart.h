/* 防止头文件被重复包含。 */
#ifndef __USART_H__
#define __USART_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* USART1 的 HAL 句柄，保存串口配置和运行状态。 */
extern UART_HandleTypeDef huart1;

/* 初始化 USART1：PA9 发送，PA10 接收，115200 8N1。 */
void MX_USART1_UART_Init(void);

/* 通过 USART1 发送一段以 \0 结尾的调试字符串。 */
HAL_StatusTypeDef Debug_UART_Send_String(const char *text);

#ifdef __cplusplus
}
#endif

#endif /* __USART_H__ */
