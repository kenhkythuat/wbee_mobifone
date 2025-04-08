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
char rx_buffer_ec[20];
char rx_buffer_ph[20];
char rx_buffer_do[100];

// uint8_t reordered_data_ph[4];
// uint8_t reordered_data_ec[4];

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
  if (huart->Instance == USART1) {
    printf("\r\nSIMCOM Response:");
    printf(rx_buffer);
    snprintf(rx_data_sim, sizeof(rx_buffer), "%s", rx_buffer);
    HAL_UARTEx_ReceiveToIdle_IT(&huart1, (uint8_t *)rx_buffer, 700);
  }
  if (huart->Instance == USART2) {
    // sensor EC
    HAL_UARTEx_ReceiveToIdle_IT(&huart2, (uint8_t *)rx_buffer_ec, 20);
    //    HAL_UARTEx_ReceiveToIdle_IT(&huart2, (uint8_t *)rx_buffer_do, 100);
  }
  if (huart->Instance == UART4) {
    // sensor PH
    HAL_UARTEx_ReceiveToIdle_IT(&huart4, (uint8_t *)rx_buffer_ph, 20);
  }
  memset(rx_buffer, '\0', 700);
}
