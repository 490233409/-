#ifndef __GPIO_OUTPUT_H__
#define __GPIO_OUTPUT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* 当前支持 2 路数字输出，通道编号为 0 和 1。 */
#define GPIO_OUTPUT_CHANNEL_COUNT 2U

/* 初始化 PB0、PB1 为推挽输出，上电默认全部断开。 */
void MX_GPIO_Output_Init(void);

/* 设置某一路输出。channel 为 0 或 1，state 为 0 断开、1 闭合。 */
HAL_StatusTypeDef GPIO_Output_Set(uint8_t channel, uint8_t state);

/* 读取某一路当前输出状态。返回 0 或 1，通道非法时返回 0。 */
uint8_t GPIO_Output_Get(uint8_t channel);

/* 把 2 路状态压成一个位图：bit0 对应通道 0，bit1 对应通道 1。 */
uint8_t GPIO_Output_Get_Bitmap(void);

/* 一次性设置 2 路输出，bitmap 的 bit0/bit1 分别控制两个通道。 */
HAL_StatusTypeDef GPIO_Output_Set_Bitmap(uint8_t bitmap);

/* 把所有输出立即断开，用于故障或安全停机。 */
void GPIO_Output_All_Off(void);

#ifdef __cplusplus
}
#endif

#endif /* __GPIO_OUTPUT_H__ */
