/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */
enum GmsModemState
{
	Off,
	On,
	InternetReady,
	MqttReady,
	Subscribed,
	UpdateToServer,
	DisconnectMqtt,
	SleepStm32
};
extern enum GmsModemState current_status_simcom;
extern char rx_buffer[700];
extern char rx_data_sim[700];
extern char array_json[400];
extern char rx_buffer_ec[20];
extern char rx_buffer_ph[20];
extern float data_ph_fuvitech;
extern uint32_t adc_pin_valve;
extern float data_percentage_pin;
extern uint16_t frequency_1hz;
extern bool to_send_status_to_server;
extern uint8_t total_errors;
// data EC Fuvitech
extern float data_measured_ph_fuvitech;
extern float data_temperature_ph_fuvitech;
// data EC Fuvitech
extern float data_ec_fuvitech;
extern float data_conductivity_ec_fuvitech;
extern float data_resistivity_ec_fuvitech;
extern float data_temperateure_ec_fuvitech;
extern float data_tds_ec_fuvitech;
extern float data_salinity_ec_fuvitech;


/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
void check_handle_state(enum GmsModemState status);
bool wait_for_pb_done_event(void);
bool check_signal_simcom(void);
float read_level_pin(void);

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define PWRKEY_SIMCOM_Pin GPIO_PIN_4
#define PWRKEY_SIMCOM_GPIO_Port GPIOA
#define LED_STATUS_Pin GPIO_PIN_12
#define LED_STATUS_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
