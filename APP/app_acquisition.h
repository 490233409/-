#ifndef __APP_ACQUISITION_H__
#define __APP_ACQUISITION_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* CAN 任务读取的最新模拟量快照。 */
typedef struct
{
  uint16_t raw_value;
  uint16_t millivolts;
  uint8_t valid;
  uint32_t sample_count;
  uint32_t error_count;
} APP_AcquisitionDataTypeDef;

/* 执行一次 ADC 采样并更新最新数据。 */
void APP_Acquisition_Sample(void);

/* 原子地复制最新采样快照。 */
void APP_Acquisition_Get_Latest(APP_AcquisitionDataTypeDef *data);

#ifdef __cplusplus
}
#endif

#endif /* __APP_ACQUISITION_H__ */
