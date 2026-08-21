#ifndef __WATCHDOG_H__
#define __WATCHDOG_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* IWDG 的 HAL 句柄，喂狗操作需要通过它进行。 */
extern IWDG_HandleTypeDef hiwdg;

/*
 * 被健康监测的任务编号。
 * 每个任务每运行一轮主循环，就用自己的编号“报到”一次。
 */
#define WATCHDOG_TASK_CAN         0U /* CAN_Service：正常每轮不超过 20 ms */
#define WATCHDOG_TASK_HEARTBEAT   1U /* Heartbeat：周期 1000 ms */
#define WATCHDOG_TASK_ACQUISITION 2U /* Acquisition：周期 100 ms */
#define WATCHDOG_TASK_MODBUS      3U /* Modbus：帧队列最长阻塞 500 ms */
#define WATCHDOG_TASK_COUNT       4U

/*
 * 初始化并启动独立看门狗（IWDG），同时记录本次复位原因。
 * 必须在 USART1 初始化之后调用，这样才能打印复位原因日志。
 */
void MX_IWDG_Init(void);

/* 任务报到：每轮主循环调用一次，task_id 取 WATCHDOG_TASK_xxx。 */
void Watchdog_Task_Checkin(uint8_t task_id);

/*
 * 健康闸门：只有全部被监测任务都在各自期限内报到过，才喂狗。
 * 由喂狗任务每 100 ms 调用一次。
 */
void Watchdog_Feed_If_Healthy(void);

/* 健康位图：bit0~bit3 对应 4 个任务，1 表示该任务在期限内正常报到。 */
uint8_t Watchdog_Get_Health_Bitmap(void);

/* 看门狗复位的累计次数（保存在备份寄存器，复位不丢失，断电才清零）。 */
uint16_t Watchdog_Get_Iwdg_Reset_Count(void);

/*
 * 产线测试钩子：让调用它的任务原地空转、不再报到。
 * 用于实测“任务卡死 -> 停止喂狗 -> 看门狗复位”的完整链路。
 */
void Watchdog_Trigger_Hang_Test(void);

#ifdef __cplusplus
}
#endif

#endif /* __WATCHDOG_H__ */
