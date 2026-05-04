/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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

#if defined ( __ICCARM__ )
#  define CMSE_NS_CALL  __cmse_nonsecure_call
#  define CMSE_NS_ENTRY __cmse_nonsecure_entry
#else
#  define CMSE_NS_CALL  __attribute((cmse_nonsecure_call))
#  define CMSE_NS_ENTRY __attribute((cmse_nonsecure_entry))
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32n6xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* Function pointer declaration in non-secure*/
#if defined ( __ICCARM__ )
typedef void (CMSE_NS_CALL *funcptr)(void);
#else
typedef void CMSE_NS_CALL (*funcptr)(void);
#endif

/* typedef for non-secure callback functions */
typedef funcptr funcptr_NS;

/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi);
void Test_CS_Toggle(void);
void Test_MISO(void);
/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define OSC_IN_Pin GPIO_PIN_0
#define OSC_IN_GPIO_Port GPIOH
#define SD_D0_Pin GPIO_PIN_4
#define SD_D0_GPIO_Port GPIOC
#define OSC_OUT_Pin GPIO_PIN_1
#define OSC_OUT_GPIO_Port GPIOH
#define SD_D1_Pin GPIO_PIN_5
#define SD_D1_GPIO_Port GPIOC
#define PSUEN_Pin GPIO_PIN_9
#define PSUEN_GPIO_Port GPIOC
#define SD_D2_Pin GPIO_PIN_0
#define SD_D2_GPIO_Port GPIOC
#define VCP_TX_Pin GPIO_PIN_5
#define VCP_TX_GPIO_Port GPIOE
#define SD_CK_Pin GPIO_PIN_2
#define SD_CK_GPIO_Port GPIOC
#define SD_D3_Pin GPIO_PIN_4
#define SD_D3_GPIO_Port GPIOE
#define VCP_RX_Pin GPIO_PIN_6
#define VCP_RX_GPIO_Port GPIOE
#define SD_CMD_Pin GPIO_PIN_3
#define SD_CMD_GPIO_Port GPIOC
#define SYNC_RESET_Pin GPIO_PIN_3
#define SYNC_RESET_GPIO_Port GPIOB
#define ADC_SPI_CS1_Pin GPIO_PIN_3
#define ADC_SPI_CS1_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
