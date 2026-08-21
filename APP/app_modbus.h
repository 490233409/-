#ifndef __APP_MODBUS_H__
#define __APP_MODBUS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "rs485.h"

/*
 * 本终端在 RS485 总线上的 Modbus 从站地址。
 * 主机（PLC 或电脑上的 Modbus 轮询软件）必须用这个地址呼叫我们。
 * 后续做 Flash 参数保存时，可以把它改成掉电可保存的运行参数。
 */
#define APP_MODBUS_SLAVE_ADDRESS 1U

/* 寄存器映射版本：以后修改寄存器布局时递增，方便主机识别固件版本。 */
#define APP_MODBUS_MAP_VERSION   1U

/* 本项目支持的 Modbus 功能码。 */
#define APP_MODBUS_FC_READ_COILS        0x01U /* 读线圈（输出状态） */
#define APP_MODBUS_FC_READ_HOLDING_REGS 0x03U /* 读保持寄存器（从站信息和统计） */
#define APP_MODBUS_FC_READ_INPUT_REGS   0x04U /* 读输入寄存器（模拟量采集结果） */
#define APP_MODBUS_FC_WRITE_SINGLE_COIL 0x05U /* 写单线圈（控制一路输出） */
#define APP_MODBUS_FC_WRITE_SINGLE_REG  0x06U /* 写单寄存器（命令寄存器） */

/*
 * 0x06 唯一可写的寄存器是地址 0 的“命令寄存器”。
 * 目前只接受一个命令值：0xDEAD 触发看门狗卡死测试（先回显再执行）。
 * 以后做 Flash 参数保存时，可以在这里扩展更多可写寄存器。
 */
#define APP_MODBUS_CMD_REG_ADDRESS       0x0000U
#define APP_MODBUS_CMD_WDOG_HANG_TEST    0xDEADU

/* 异常响应中使用的异常码。 */
#define APP_MODBUS_EX_ILLEGAL_FUNCTION     0x01U /* 不支持的功能码 */
#define APP_MODBUS_EX_ILLEGAL_DATA_ADDRESS 0x02U /* 寄存器/线圈地址越界 */
#define APP_MODBUS_EX_ILLEGAL_DATA_VALUE   0x03U /* 数值不符合协议规定 */

/* 输入寄存器地址（0x04 读取，只读）：来自采集任务的最新模拟量快照。 */
#define APP_MODBUS_INPUT_ADDR_ADC_RAW  0U /* ADC 原始值，范围 0~4095 */
#define APP_MODBUS_INPUT_ADDR_ADC_MV   1U /* 估算电压，单位 mV */
#define APP_MODBUS_INPUT_ADDR_VALID    2U /* 采集有效标志，1 表示有效 */
#define APP_MODBUS_INPUT_COUNT         3U

/* 保持寄存器地址（0x03 读取，只读）：从站信息、通信统计和看门狗状态。 */
#define APP_MODBUS_HOLDING_ADDR_SLAVE_ID    0U /* 从站地址 */
#define APP_MODBUS_HOLDING_ADDR_MAP_VERSION 1U /* 寄存器映射版本 */
#define APP_MODBUS_HOLDING_ADDR_REQUESTS    2U /* 收到的有效请求帧数低 16 位 */
#define APP_MODBUS_HOLDING_ADDR_CRC_ERRORS  3U /* CRC 校验失败帧数低 16 位 */
#define APP_MODBUS_HOLDING_ADDR_EXCEPTIONS  4U /* 异常响应次数低 16 位 */
#define APP_MODBUS_HOLDING_ADDR_RESPONSES   5U /* 正常响应次数低 16 位 */
#define APP_MODBUS_HOLDING_ADDR_HEALTH      6U /* 任务健康位图，0x0F 表示全部健康 */
#define APP_MODBUS_HOLDING_ADDR_IWDG_RESETS 7U /* 看门狗复位累计次数 */
#define APP_MODBUS_HOLDING_COUNT            8U

/* 线圈地址（0x01 读、0x05 写）：与 PB0/PB1 两路数字输出一一对应。 */
#define APP_MODBUS_COIL_COUNT 2U

/* 0x05 写单线圈时，协议规定只有这两种合法值。 */
#define APP_MODBUS_COIL_ON_VALUE  0xFF00U /* 闭合 */
#define APP_MODBUS_COIL_OFF_VALUE 0x0000U /* 断开 */

/* 校验通过、且确实是发给本站的请求帧数。 */
extern volatile uint32_t app_modbus_request_count;

/* 成功发出的正常响应帧数。 */
extern volatile uint32_t app_modbus_response_count;

/* CRC 校验失败而被静默丢弃的帧数。 */
extern volatile uint32_t app_modbus_crc_error_count;

/* 已发出的异常响应帧数。 */
extern volatile uint32_t app_modbus_exception_count;

/* 处理 Modbus 任务从帧队列取出的一帧请求。 */
void APP_Modbus_Process_Frame(const RS485_FrameTypeDef *frame);

#ifdef __cplusplus
}
#endif

#endif /* __APP_MODBUS_H__ */
