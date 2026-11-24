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

/* Includes ------------------------------------------------------------------*/
#include "stm32l4xx_hal.h"

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
#define ISM43362_RST_Pin GPIO_PIN_8
#define ISM43362_RST_GPIO_Port GPIOE
#define INTERNAL_I2C2_SCL_Pin GPIO_PIN_10
#define INTERNAL_I2C2_SCL_GPIO_Port GPIOB
#define INTERNAL_I2C2_SDA_Pin GPIO_PIN_11
#define INTERNAL_I2C2_SDA_GPIO_Port GPIOB
#define ISM43362_BOOT0_Pin GPIO_PIN_12
#define ISM43362_BOOT0_GPIO_Port GPIOB
#define ISM43362_WAKEUP_Pin GPIO_PIN_13
#define ISM43362_WAKEUP_GPIO_Port GPIOB
#define SYS_JTMS_SWDIO_Pin GPIO_PIN_13
#define SYS_JTMS_SWDIO_GPIO_Port GPIOA
#define SYS_JTCK_SWCLK_Pin GPIO_PIN_14
#define SYS_JTCK_SWCLK_GPIO_Port GPIOA
#define ISM43362_SPI3_SCK_Pin GPIO_PIN_10
#define ISM43362_SPI3_SCK_GPIO_Port GPIOC
#define ISM43362_SPI3_MISO_Pin GPIO_PIN_11
#define ISM43362_SPI3_MISO_GPIO_Port GPIOC
#define ISM43362_SPI3_MOSI_Pin GPIO_PIN_12
#define ISM43362_SPI3_MOSI_GPIO_Port GPIOC
#define ST_LINK_UART1_TX_Pin GPIO_PIN_6
#define ST_LINK_UART1_TX_GPIO_Port GPIOB
#define ST_LINK_UART1_RX_Pin GPIO_PIN_7
#define ST_LINK_UART1_RX_GPIO_Port GPIOB
#define ISM43362_SPI3_CSN_Pin GPIO_PIN_0
#define ISM43362_SPI3_CSN_GPIO_Port GPIOE
#define ISM43362_DRDY_EXTI1_Pin GPIO_PIN_1
#define ISM43362_DRDY_EXTI1_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
