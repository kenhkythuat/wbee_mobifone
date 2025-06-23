/*
 * convert_data_uart.c
 *
 *  Created on: Mar 3, 2025
 *      Author: thuanphat
 */
#include "cJSON.h"
#include "config.h"
#include "main.h"
#include "stdbool.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include <math.h>
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart4;
extern bool is_publish_data_lcd;
char rx_buffer_ec[20];
char rx_buffer_ph[20];
char rx_buffer_do[100];
char rx_buffer_fuvitech[100];
char data_status_pump[50]="{\"1\":%d,\"2\":%d,\"3\":%d}";
char tx5_status_pump[50];

uint8_t payLoadPin;

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
  if (huart->Instance == USART1) {
//    printf("\r\nSIMCOM Response:");
//    printf(rx_buffer);

#if INTERVAL_PUPLISH_DATA >= 60
    {
    	snprintf(rx_data_sim, sizeof(rx_buffer), "%s", rx_buffer);
    }
#else
    for (int i = 0; i < 700; i++) {
      rx_data_sim[i] = rx_buffer[i];
    if ((char)rx_buffer[i] == (char)SERIAL_NUMBER[5] && (char)rx_buffer[i + 1] == (char)SERIAL_NUMBER[6] &&
        (char)rx_buffer[i + 2] == (char)SERIAL_NUMBER[7]) {
      payLoadPin = (rx_buffer[i + 4] - 48);
#if SIMCOM_MODEL == a7672s
      if (rx_buffer[(i + 29)] == 49 && is_pb_done == true)
#elif SIMCOM_MODEL == a7670c
      if (rx_buffer[(i + 31)] == 49 && is_pb_done == true)
#elif SIMCOM_MODEL == a7670sa
      if (rx_buffer[(i + 29)] == 49 && is_pb_done == true)
#elif SIMCOM_MODEL == a7680
      if (rx_buffer[(i + 29)] == 49 && is_pb_done == true)
#endif
      {
        printf("-----------ON RELAY %d -----------\r\n", payLoadPin);
        if(payLoadPin==1)
        {
        	 HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
        	__HAL_TIM_SET_COMPARE(&htim2,TIM_CHANNEL_3, duty_cycles_ec);
        	motor_ec=1;
        }
        if(payLoadPin==2)
        {
        	 HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);
        	__HAL_TIM_SET_COMPARE(&htim2,TIM_CHANNEL_3, duty_cycles_ph);
        	motor_ph_1=1;
        }
        if(payLoadPin==3)
        {
        	 HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
        	__HAL_TIM_SET_COMPARE(&htim2,TIM_CHANNEL_3, duty_cycles_x);
        	motor_ph_2=1;
        }
        sprintf(tx5_status_pump,data_status_pump,motor_ec,motor_ph_1,motor_ph_2);
        is_publish_data_lcd = update_data_to_sreen((uint8_t *)tx5_status_pump);
      }

#if SIMCOM_MODEL == a7672s
      if (rx_buffer[(i + 29)] == 48 && is_pb_done == true)
#elif SIMCOM_MODEL == a7670c
      if (rx_buffer[(i + 31)] == 48 && is_pb_done == true)
#elif SIMCOM_MODEL == a7670sa
      if (rx_buffer[(i + 29)] == 48 && is_pb_done == true)
#elif SIMCOM_MODEL == a7680
      if (rx_buffer[(i + 29)] == 48 && is_pb_done == true)
#endif
      {
        printf("-----------OFF RELAY %d -----------\r\n", payLoadPin);
        if(payLoadPin==1){
        	HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_3);
        	motor_ec=0;
        }
        if(payLoadPin==2){
        	HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_4);
        	motor_ph_1=0;
        }
        if(payLoadPin==3){
        	HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
        	motor_ph_2=0;
        }
        sprintf(tx5_status_pump,data_status_pump,motor_ec,motor_ph_1,motor_ph_2);
        is_publish_data_lcd = update_data_to_sreen((uint8_t *)tx5_status_pump);
      }

    }
  }
#endif
    if ((strstr((char *)rx_buffer, "+CMQTTCONNLOST") != NULL) && is_pb_done == true) {
      printf("--------------Client Disconnect passively!---------------\n");
      current_status_simcom = On;
    }
  }
  if (huart->Instance == USART2) {
        HAL_UARTEx_ReceiveToIdle_IT(&huart2, (uint8_t *)rx_buffer_fuvitech,100);
  }
  if (huart->Instance == UART4) {
    // sensor PH
    HAL_UARTEx_ReceiveToIdle_IT(&huart4, (uint8_t *)rx_buffer_ph, 20);
  }
  memset(rx_buffer, '\0', 700);
  HAL_UARTEx_ReceiveToIdle_IT(&huart1, (uint8_t *)rx_buffer, 700);
}
