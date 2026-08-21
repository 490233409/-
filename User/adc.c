#include "adc.h"

ADC_HandleTypeDef hadc1;

void MX_ADC1_Init(void)
{
  ADC_ChannelConfTypeDef channel_config = {0};

  /* F1的ADC时钟不能超14MHz，72/6=12MHz */
  __HAL_RCC_ADC_CONFIG(RCC_ADCPCLK2_DIV6);

  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;

  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /* 通道0对应PA0，电位器阻值大，采样时间放长点稳 */
  channel_config.Channel = ADC_CHANNEL_0;
  channel_config.Rank = ADC_REGULAR_RANK_1;
  channel_config.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;

  if (HAL_ADC_ConfigChannel(&hadc1, &channel_config) != HAL_OK)
  {
    Error_Handler();
  }

  /* F1的ADC上电要先校准，不校准误差挺大 */
  if (HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }
}

void HAL_ADC_MspInit(ADC_HandleTypeDef *adc_handle)
{
  GPIO_InitTypeDef gpio_init = {0};

  if (adc_handle->Instance == ADC1)
  {
    __HAL_RCC_ADC1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio_init.Pin = ADC_INPUT_Pin;
    gpio_init.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(ADC_INPUT_GPIO_Port, &gpio_init);
  }
}

void HAL_ADC_MspDeInit(ADC_HandleTypeDef *adc_handle)
{
  if (adc_handle->Instance == ADC1)
  {
    __HAL_RCC_ADC1_CLK_DISABLE();
    HAL_GPIO_DeInit(ADC_INPUT_GPIO_Port, ADC_INPUT_Pin);
  }
}

HAL_StatusTypeDef ADC_Read_Raw(uint16_t *raw_value)
{
  HAL_StatusTypeDef status;

  if (raw_value == NULL)
  {
    return HAL_ERROR;
  }

  status = HAL_ADC_Start(&hadc1);
  if (status != HAL_OK)
  {
    return status;
  }

  /* 正常几十微秒就完了，10ms纯属保险 */
  status = HAL_ADC_PollForConversion(&hadc1, 10U);
  if (status == HAL_OK)
  {
    *raw_value = (uint16_t)HAL_ADC_GetValue(&hadc1);
  }

  if (HAL_ADC_Stop(&hadc1) != HAL_OK)
  {
    return HAL_ERROR;
  }

  return status;
}
