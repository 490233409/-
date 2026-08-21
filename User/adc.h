#ifndef __ADC_H__
#define __ADC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* ADC1 的 HAL 句柄，保存 ADC 配置和运行状态。 */
extern ADC_HandleTypeDef hadc1;

/* 初始化 ADC1 通道 0，也就是 PA0，并执行一次硬件校准。 */
void MX_ADC1_Init(void);

/* 触发一次 ADC 转换并读取 0~4095 的原始值。 */
HAL_StatusTypeDef ADC_Read_Raw(uint16_t *raw_value);

#ifdef __cplusplus
}
#endif

#endif /* __ADC_H__ */
