/*
 * convert_data_uart.c
 *
 *  Created on: Mar 3, 2025
 *      Author: thuanphat
 */
#include "cJSON.h"
#include "cfg_store.h"
#include "config.h"
#include "main.h"
#include "stdbool.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include <math.h>
#include "ota_update.h"
#include "plc_rs485.h"
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart4;
extern UART_HandleTypeDef huart5;
extern bool is_publish_data_lcd;
char rx_buffer_ec[20];
char rx_buffer_ph[20];
char rx_buffer_do[100];
char rx_buffer_fuvitech[100];
char data_status_pump[50]="{\"deviceID\":\"%s\",\"1\":%d,\"2\":%d,\"3\":%d}\r\n";
char tx5_status_pump[96];
int temp_cycle=duty_cycles_ph;

uint8_t payLoadPin;


 uint8_t  rx_idle_buf[RX_IDLE_BUF_SZ];

 volatile uint16_t g_rx_len  = 0;
 volatile uint8_t  g_rx_flag = 0;      // cờ báo có dữ liệu mới
 char g_rx_line[RX_LINE_MAX];          // chuỗi nhận được (null-terminated)

static bool copy_json_string_value(const char *json, const char *key,
                                   char *out, size_t out_size) {
  char pattern[32];
  const char *value;
  size_t length = 0U;

  if (json == NULL || key == NULL || out == NULL || out_size == 0U) {
    return false;
  }
  out[0] = '\0';
  if (snprintf(pattern, sizeof(pattern), "\"%s\"", key) >=
      (int)sizeof(pattern)) {
    return false;
  }

  value = strstr(json, pattern);
  if (value == NULL) {
    return false;
  }
  value += strlen(pattern);
  while (*value == ' ' || *value == '\t') {
    value++;
  }
  if (*value++ != ':') {
    return false;
  }
  while (*value == ' ' || *value == '\t') {
    value++;
  }
  if (*value++ != '"') {
    return false;
  }

  while (value[length] != '\0' && value[length] != '"' &&
         value[length] != '\r' && value[length] != '\n') {
    if (value[length] == '\\' || length >= (out_size - 1U)) {
      out[0] = '\0';
      return false;
    }
    out[length] = value[length];
    length++;
  }
  if (value[length] != '"') {
    out[0] = '\0';
    return false;
  }
  out[length] = '\0';
  return true;
}

void uart5_rx_start_to_idle(void)
{
    HAL_UARTEx_ReceiveToIdle_IT(&huart5, rx_idle_buf, RX_IDLE_BUF_SZ);

    // hay dùng để bỏ half-transfer (nếu RX dùng DMA)
    if (huart5.hdmarx) {
        __HAL_DMA_DISABLE_IT(huart5.hdmarx, DMA_IT_HT);
    }
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
  if (huart->Instance == USART1) {
    uint16_t text_size = Size;
    const char *serial_number = device_serial_get();
    size_t serial_length = strlen(serial_number);

    ota_uart_rx_capture((const uint8_t *)rx_buffer, Size);
    if (text_size >= sizeof(rx_buffer)) {
      text_size = sizeof(rx_buffer) - 1U;
    }
    rx_buffer[text_size] = '\0';
    memcpy(rx_data_sim, rx_buffer, text_size + 1U);
//    printf("\r\nSIMCOM Response:");
//    printf(rx_buffer);

#if INTERVAL_PUPLISH_DATA < 60
    for (int i = 0; i < (int)text_size - 31; i++) {
    if (serial_length >= 3U &&
        (char)rx_buffer[i] == serial_number[serial_length - 3U] &&
        (char)rx_buffer[i + 1] == serial_number[serial_length - 2U] &&
        (char)rx_buffer[i + 2] == serial_number[serial_length - 1U]) {
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
        	temp_cycle=temp_cycle+10;
        	if(temp_cycle>100){
        		temp_cycle=100;
        	}
			 HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);
			__HAL_TIM_SET_COMPARE(&htim2,TIM_CHANNEL_4, duty_cycles_ph);
			motor_ph_plus=1;
        }
        if(payLoadPin==2)
        {
			 HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
			__HAL_TIM_SET_COMPARE(&htim2,TIM_CHANNEL_3, duty_cycles_ec);
			motor_ph_minus=1;
        }
        if(payLoadPin==3)
        {
        	 HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
        	__HAL_TIM_SET_COMPARE(&htim2,TIM_CHANNEL_1, duty_cycles_x);
        	motor_x=1;
        }
        snprintf(tx5_status_pump, sizeof(tx5_status_pump), data_status_pump,
                 device_serial_get(), motor_ph_plus, motor_ph_minus, motor_x);
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
        	HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_4);
        	motor_ph_plus=0;
        }
        if(payLoadPin==2){
        	HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_3);
        	motor_ph_minus=0;
        }
        if(payLoadPin==3){
        	HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
        	motor_x=0;
        }
        snprintf(tx5_status_pump, sizeof(tx5_status_pump), data_status_pump,
                 device_serial_get(), motor_ph_plus, motor_ph_minus, motor_x);
        is_publish_data_lcd = update_data_to_sreen((uint8_t *)tx5_status_pump);
      }

    }
  }
#endif
    if ((strstr((char *)rx_buffer, "+CMQTTCONNLOST") != NULL) && is_pb_done == true) {
      printf("--------------Client Disconnect passively!---------------\n");
      current_status_simcom = On;
    }
    if (strstr((char *)rx_buffer, "set_serial_number") != NULL &&
        is_pb_done == true) {
      copy_json_string_value((char *)rx_buffer, "request_id",
                             serial_request_id,
                             sizeof(serial_request_id));
      copy_json_string_value((char *)rx_buffer, "serial_number",
                             requested_serial_number,
                             sizeof(requested_serial_number));
      printf("----------Set serial number request received----------\r\n");
      to_change_serial_number = true;
    }
    const char *ota_command = NULL;
    if (strstr((char *)rx_buffer, "ota_update") != NULL) {
      ota_command = "ota_update";
    } else if (strstr((char *)rx_buffer, "ota_check") != NULL) {
      ota_command = "ota_check";
    }
    if (ota_command != NULL && is_pb_done == true) {
      snprintf(ota_request_command, sizeof(ota_request_command), "%s",
               ota_command);
      copy_json_string_value((char *)rx_buffer, "request_id", ota_request_id,
                             sizeof(ota_request_id));
      printf("--------------OTA request received---------------\r\n");
      to_start_ota = true;
    }
  }
  else if (huart->Instance == UART5)
  {
      // ===== THÊM MỚI CHO UART5 (ESP32 -> STM32) =====
	  printf("-----------UART IT 5 -----------\r\n");
      if (Size >= RX_LINE_MAX) Size = RX_LINE_MAX - 1;

      memcpy(g_rx_line, rx_idle_buf, Size);
      g_rx_line[Size] = '\0';

      g_rx_len  = Size;
      g_rx_flag = 1;

      uart5_rx_start_to_idle(); // re-arm
  }
  else if (huart->Instance == USART2) {
#if SENSOR_DATA_SOURCE == SENSOR_SOURCE_PLC_RS485
        plc_rs485_on_uart_rx((uint8_t *)rx_buffer_fuvitech, Size);
#endif
        HAL_UARTEx_ReceiveToIdle_IT(&huart2, (uint8_t *)rx_buffer_fuvitech,100);
  }
  else if (huart->Instance == UART4) {
    // sensor PH
    HAL_UARTEx_ReceiveToIdle_IT(&huart4, (uint8_t *)rx_buffer_ph, 20);
  }
  memset(rx_buffer, '\0', 700);
  HAL_UARTEx_ReceiveToIdle_IT(&huart1, (uint8_t *)rx_buffer, 700);
}
