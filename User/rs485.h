#ifndef __RS485_H__
#define __RS485_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "FreeRTOS.h"

/* USART2 连接 RS485 模块的 TXD/RXD。 */
extern UART_HandleTypeDef huart2;

/* RS485 接收统计和最近一次接收数据。 */
typedef struct
{
  uint32_t rx_count;
  uint32_t tx_count;
  uint32_t error_count;
  uint32_t last_error;
  uint8_t last_rx_byte;
  /* 下面是 Modbus 帧级统计，CAN 的 RS485_STATUS 命令暂不使用。 */
  uint32_t rx_frame_count;             /* 成功进入帧队列的完整帧数 */
  uint32_t frame_queue_overflow_count; /* 帧队列已满而丢弃的新帧数 */
  uint32_t frame_too_long_count;       /* 超过最大帧长被整帧丢弃的次数 */
} RS485_StatusTypeDef;

/*
 * Modbus RTU 一帧最长 256 字节：
 * 1 字节从站地址 + 最多 253 字节 PDU + 2 字节 CRC16。
 */
#define RS485_FRAME_MAX_SIZE 256U

/*
 * FreeRTOS 帧队列的深度。
 * Modbus 是问答式协议，主机发一帧、等一帧，2 帧深度已经能覆盖突发情况。
 */
#define RS485_FRAME_QUEUE_SIZE 2U

/*
 * 把一帧完整报文的长度和服务数据放在一起，
 * Modbus 任务取帧时只需要一个变量，与 CAN_ReceivedMessageTypeDef 思路一致。
 */
typedef struct
{
  uint16_t length;                    /* 本帧实际收到的字节数 */
  uint8_t data[RS485_FRAME_MAX_SIZE]; /* 本帧的全部字节 */
} RS485_FrameTypeDef;

/* 初始化 USART2、PB10 方向控制，并启动单字节接收中断。 */
void MX_RS485_UART_Init(void);

/* 发送一段 RS485 数据。该函数只能在任务环境中调用，不能在中断中调用。 */
HAL_StatusTypeDef RS485_Send(const uint8_t *data, uint16_t length);

/* 安全复制当前 RS485 收发状态。 */
void RS485_Get_Status(RS485_StatusTypeDef *status);

/* 清零 RS485 收发统计，不改变当前硬件工作状态。 */
void RS485_Clear_Statistics(void);

/* 创建 Modbus 帧接收队列，必须在打开调度器前调用。pdPASS 表示创建成功。 */
BaseType_t RS485_FrameQueue_Init(void);

/* 阻塞等待一帧完整报文，等待时间使用 FreeRTOS tick 表示。 */
BaseType_t RS485_Wait_Frame(RS485_FrameTypeDef *frame, TickType_t wait_ticks);

/*
 * 总线空闲（IDLE）中断的处理函数，只能被 USART2_IRQHandler 调用。
 * 它把刚收完的一帧放入队列，并重新开始接收下一帧。
 */
void RS485_IdleDetectedFromISR(void);

#ifdef __cplusplus
}
#endif

#endif /* __RS485_H__ */
