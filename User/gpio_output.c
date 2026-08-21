#include "gpio_output.h"

/* 通道和引脚的对应表，以后加第3路只改这里 */
typedef struct
{
  GPIO_TypeDef *port;
  uint16_t pin;
} GPIO_OutputChannelTypeDef;

static const GPIO_OutputChannelTypeDef output_channels[GPIO_OUTPUT_CHANNEL_COUNT] =
{
  { OUTPUT1_GPIO_Port, OUTPUT1_Pin },  /* PB0 */
  { OUTPUT2_GPIO_Port, OUTPUT2_Pin }   /* PB1 */
};

static uint8_t output_states[GPIO_OUTPUT_CHANNEL_COUNT] = {0};

/* 先拉低再配输出，不然上电瞬间继电器会抖一下 */
void MX_GPIO_Output_Init(void)
{
  GPIO_InitTypeDef gpio_init = {0};
  uint8_t index;

  __HAL_RCC_GPIOB_CLK_ENABLE();

  for (index = 0U; index < GPIO_OUTPUT_CHANNEL_COUNT; index++)
  {
    HAL_GPIO_WritePin(output_channels[index].port,
                      output_channels[index].pin,
                      GPIO_PIN_RESET);

    gpio_init.Pin = output_channels[index].pin;
    gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(output_channels[index].port, &gpio_init);

    output_states[index] = 0U;
  }
}

HAL_StatusTypeDef GPIO_Output_Set(uint8_t channel, uint8_t state)
{
  if (channel >= GPIO_OUTPUT_CHANNEL_COUNT)
  {
    return HAL_ERROR;
  }

  if (state != 0U)
  {
    state = 1U;
  }

  HAL_GPIO_WritePin(output_channels[channel].port,
                    output_channels[channel].pin,
                    (state != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);

  output_states[channel] = state;

  return HAL_OK;
}

uint8_t GPIO_Output_Get(uint8_t channel)
{
  if (channel >= GPIO_OUTPUT_CHANNEL_COUNT)
  {
    return 0U;
  }

  return output_states[channel];
}

uint8_t GPIO_Output_Get_Bitmap(void)
{
  uint8_t bitmap = 0U;
  uint8_t index;

  for (index = 0U; index < GPIO_OUTPUT_CHANNEL_COUNT; index++)
  {
    if (output_states[index] != 0U)
    {
      bitmap |= (uint8_t)(1U << index);
    }
  }

  return bitmap;
}

HAL_StatusTypeDef GPIO_Output_Set_Bitmap(uint8_t bitmap)
{
  uint8_t index;
  uint8_t state;

  for (index = 0U; index < GPIO_OUTPUT_CHANNEL_COUNT; index++)
  {
    state = (uint8_t)((bitmap >> index) & 0x01U);

    if (GPIO_Output_Set(index, state) != HAL_OK)
    {
      return HAL_ERROR;
    }
  }

  return HAL_OK;
}

/* 出故障时全部断开 */
void GPIO_Output_All_Off(void)
{
  uint8_t index;

  for (index = 0U; index < GPIO_OUTPUT_CHANNEL_COUNT; index++)
  {
    (void)GPIO_Output_Set(index, 0U);
  }
}
