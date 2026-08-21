#include "app_acquisition.h"

#include "FreeRTOS.h"
#include "task.h"
#include "adc.h"

#define ADC_REFERENCE_MV 3300U
#define ADC_FULL_SCALE   4095U

/* 只有采集任务写，别的任务读，读写都进临界区保证快照是完整的 */
static APP_AcquisitionDataTypeDef latest_data = {0};

void APP_Acquisition_Sample(void)
{
  uint16_t raw_value;
  uint16_t millivolts;

  if (ADC_Read_Raw(&raw_value) == HAL_OK)
  {
    /* 先转32位再乘，16位乘3300会溢出；+2048是为了四舍五入 */
    millivolts = (uint16_t)((((uint32_t)raw_value * ADC_REFERENCE_MV) +
                              (ADC_FULL_SCALE / 2U)) /
                             ADC_FULL_SCALE);

    taskENTER_CRITICAL();
    latest_data.raw_value = raw_value;
    latest_data.millivolts = millivolts;
    latest_data.valid = 1U;
    latest_data.sample_count++;
    taskEXIT_CRITICAL();
  }
  else
  {
    taskENTER_CRITICAL();
    latest_data.valid = 0U;
    latest_data.error_count++;
    taskEXIT_CRITICAL();
  }
}

void APP_Acquisition_Get_Latest(APP_AcquisitionDataTypeDef *data)
{
  if (data == NULL)
  {
    return;
  }

  taskENTER_CRITICAL();
  *data = latest_data;
  taskEXIT_CRITICAL();
}
