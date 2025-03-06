/*
 * sensor.c
 *
 *  Created on: Mar 3, 2025
 *      Author: thuanphat
 */
/* USER CODE BEGIN Includes */
#include "config.h"
#include "stdbool.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include <main.h>
#include <stdbool.h>
/* USER CODE END Includes */
/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart4;
/* USER CODE END EM */
// data PH Fuvitech
float data_ph_fuvitech;
float data_measured_ph_fuvitech;
float data_temperature_ph_fuvitech;
// data EC Fuvitech
float data_ec_fuvitech;
float data_conductivity_ec_fuvitech;
float data_resistivity_ec_fuvitech;
float data_temperateure_ec_fuvitech;
float data_tds_ec_fuvitech;
float data_salinity_ec_fuvitech;

float ieee754_to_float(uint8_t *bytes) {
  uint32_t int_representation = 0;
  // Chuyển mảng byte thành số nguyên 32-bit
  int_representation = (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
  // Ép kiểu về float
  float result;
  memcpy(&result, &int_representation, sizeof(result));
  return result;
}

#if ph_fuvitech
unsigned char measured_command_ph_fuvitech[8] = {0x01, 0x03, 0x00, 0x01, 0x00, 0x02, 0x95, 0xCB};
unsigned char temperature_command_ph_fuvitech[8] = {0x01, 0x03, 0x00, 0x03, 0x00, 0x02, 0x34, 0x0B};
#endif
#if ph_fuvitech
unsigned char conductivity_command_ec_fuvitech[8] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC4, 0x0B};
unsigned char resistivity_command_ec_fuvitech[8] = {0x01, 0x03, 0x00, 0x02, 0x00, 0x02, 0x65, 0xCB};
unsigned char temperateure_command_ec_fuvitech[8] = {0x01, 0x03, 0x00, 0x04, 0x00, 0x02, 0x85, 0xCA};
unsigned char tds_command_ec_fuvitech[8] = {0x01, 0x03, 0x00, 0x06, 0x00, 0x02, 0x24, 0x0A};
unsigned char salinity_command_ec_fuvitech[8] = {0x01, 0x03, 0x00, 0x08, 0x00, 0x02, 0x45, 0xC9};
#endif
float read_ph_fuvitech(uint8_t *data) {
  HAL_UART_Transmit(&huart4, data, strlen((char *)data), 2000);
  HAL_Delay(1000);
  uint8_t reordered_data[4] = {rx_buffer_ph[5], rx_buffer_ph[6], rx_buffer_ph[3], rx_buffer_ph[4]};
  data_ph_fuvitech = ieee754_to_float(reordered_data);
  return data_ph_fuvitech;
}

float read_ec_fuvitech(uint8_t *data) {
  HAL_UART_Transmit(&huart2, data, strlen((char *)data), 2000);
  HAL_Delay(1000);
  uint8_t reordered_data[4] = {rx_buffer_ec[5], rx_buffer_ec[6], rx_buffer_ec[3], rx_buffer_ec[4]};
  data_ec_fuvitech = ieee754_to_float(reordered_data);
  return data_ec_fuvitech;
}

void read_sensor(void) {
  // read PH Fuvitech
  data_measured_ph_fuvitech = read_ec_fuvitech(measured_command_ph_fuvitech);
  data_temperature_ph_fuvitech = read_ec_fuvitech(temperature_command_ph_fuvitech);
  // read EC Fuvitech
  data_conductivity_ec_fuvitech = read_ph_fuvitech(conductivity_command_ec_fuvitech);
  data_resistivity_ec_fuvitech = read_ph_fuvitech(resistivity_command_ec_fuvitech);
  data_temperateure_ec_fuvitech = read_ph_fuvitech(temperateure_command_ec_fuvitech);
  data_tds_ec_fuvitech = read_ph_fuvitech(tds_command_ec_fuvitech);
  data_salinity_ec_fuvitech = read_ph_fuvitech(tds_command_ec_fuvitech);
}
