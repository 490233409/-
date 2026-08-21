/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __CAN_H__
#define __CAN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "FreeRTOS.h"

/*
 * 把一帧接收报文的头部和数据放在同一个结构体中，
 * 应用层取消息时只需要传递一个变量。
 */
typedef struct
{
  CAN_RxHeaderTypeDef header;
  uint8_t data[8];
} CAN_ReceivedMessageTypeDef;

/* FreeRTOS 接收队列最多保存 16 帧报文。 */
#define CAN_RX_QUEUE_SIZE 16U

extern CAN_HandleTypeDef hcan;

/* 每成功接收一帧报文，该计数器加 1。 */
extern volatile uint32_t can_rx_count;

/* 记录从 FIFO0 读取报文失败的次数。 */
extern volatile uint32_t can_rx_error_count;

/* 队列已满而丢弃新报文的次数。 */
extern volatile uint32_t can_rx_queue_overflow_count;

/* CAN 错误中断累计次数。 */
extern volatile uint32_t can_error_interrupt_count;

/* 进入 Bus-Off 状态的累计次数。 */
extern volatile uint32_t can_bus_off_count;

/* 从 Bus-Off 成功恢复的累计次数。 */
extern volatile uint32_t can_bus_off_recovery_count;

/* 软件恢复尝试失败的累计次数。 */
extern volatile uint32_t can_bus_off_recovery_failure_count;

/* 最近一次 HAL CAN 错误码。 */
extern volatile uint32_t can_last_error_code;

void MX_CAN_Init(void);

/* 创建容量为 16 帧的 FreeRTOS CAN 接收队列。 */
BaseType_t CAN_RxQueue_Init(void);

/* 配置 CAN 接收过滤器。返回 HAL_OK 表示配置成功。 */
HAL_StatusTypeDef CAN_Filter_Config(void);

/* 启动 CAN 外设。完成这一步后，CAN 才能真正收发报文。 */
HAL_StatusTypeDef CAN_Start(void);

/* 发送一帧固定的测试报文，供 USB-CAN 联调使用。 */
HAL_StatusTypeDef CAN_Send_Test_Message(void);

/* 发送一帧 11 位标准 CAN 数据帧。 */
HAL_StatusTypeDef CAN_Send_Standard_Message(uint16_t standard_id,
                                            const uint8_t *data,
                                            uint8_t length);

/* 阻塞等待一帧 CAN 报文，等待时间使用 FreeRTOS tick 表示。 */
BaseType_t CAN_Wait_Received_Message(CAN_ReceivedMessageTypeDef *message,
                                     TickType_t wait_ticks);

/* 在 FreeRTOS CAN 服务任务中检查并处理 Bus-Off 自动恢复。 */
void CAN_Process_Recovery(void);

#ifdef __cplusplus
}
#endif

#endif /* __CAN_H__ */
