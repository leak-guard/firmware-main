/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED_IMP_Pin GPIO_PIN_0
#define LED_IMP_GPIO_Port GPIOC
#define LED_WIFI_Pin GPIO_PIN_1
#define LED_WIFI_GPIO_Port GPIOC
#define LED_ERROR_Pin GPIO_PIN_2
#define LED_ERROR_GPIO_Port GPIOC
#define LED_VALVE_Pin GPIO_PIN_3
#define LED_VALVE_GPIO_Port GPIOC
#define LORA_RESET_Pin GPIO_PIN_3
#define LORA_RESET_GPIO_Port GPIOA
#define LORA_NSS_Pin GPIO_PIN_4
#define LORA_NSS_GPIO_Port GPIOA
#define LED_OK_Pin GPIO_PIN_4
#define LED_OK_GPIO_Port GPIOC
#define FLOW_IMP_IN_Pin GPIO_PIN_7
#define FLOW_IMP_IN_GPIO_Port GPIOE
#define FLOW_BTN_IN_Pin GPIO_PIN_8
#define FLOW_BTN_IN_GPIO_Port GPIOE
#define LORA_DIO0_Pin GPIO_PIN_12
#define LORA_DIO0_GPIO_Port GPIOB
#define LORA_DIO0_EXTI_IRQn EXTI15_10_IRQn
#define LORA_DIO1_Pin GPIO_PIN_13
#define LORA_DIO1_GPIO_Port GPIOB
#define LORA_DIO2_Pin GPIO_PIN_14
#define LORA_DIO2_GPIO_Port GPIOB
#define LORA_DIO3_Pin GPIO_PIN_15
#define LORA_DIO3_GPIO_Port GPIOB
#define BUZZER_OUT_Pin GPIO_PIN_6
#define BUZZER_OUT_GPIO_Port GPIOC
#define BTN_UNLOCK_Pin GPIO_PIN_0
#define BTN_UNLOCK_GPIO_Port GPIOD
#define EEPROM_WP_Pin GPIO_PIN_6
#define EEPROM_WP_GPIO_Port GPIOD
#define VALVE_OUT_Pin GPIO_PIN_0
#define VALVE_OUT_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
