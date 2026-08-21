/**
  ******************************************************************************
  * @file    Templates/Inc/main.h 
  * @author  MCD Application Team
  * @brief   Header for main.c module
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2016 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
  
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* CAN1 uses the default STM32F103 mapping. */
#define CAN_RX_Pin GPIO_PIN_11
#define CAN_RX_GPIO_Port GPIOA
#define CAN_TX_Pin GPIO_PIN_12
#define CAN_TX_GPIO_Port GPIOA

/* 电位器 SIG 接到 ADC1 通道 0，也就是 PA0。 */
#define ADC_INPUT_Pin GPIO_PIN_0
#define ADC_INPUT_GPIO_Port GPIOA

/* 2 路数字输出，高电平表示闭合，可接继电器模块或 LED。 */
#define OUTPUT1_Pin GPIO_PIN_0
#define OUTPUT1_GPIO_Port GPIOB
#define OUTPUT2_Pin GPIO_PIN_1
#define OUTPUT2_GPIO_Port GPIOB

/* RS485 使用 USART2：PA2 发送、PA3 接收，PB10 控制模块 DIR。 */
#define RS485_DIR_Pin GPIO_PIN_10
#define RS485_DIR_GPIO_Port GPIOB

void Error_Handler(void);
/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
