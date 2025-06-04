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

// data DO Fuvitech
float data_do_fuvitech;
float data_dissolved_oxygen_fuvitech;
float data_temperature_do_fuvitech;

float ieee754_to_float(unsigned char *bytes) {
  uint32_t int_representation = 0;
  // Chuyển mảng byte thành số nguyên 32-bit
  int_representation = (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
  // Ép kiểu về float
  float result;
  memcpy(&result, &int_representation, sizeof(result));
  return result;
}

float bytes_to_float(unsigned char *byte_array) {
  unsigned int temp = 0;

  // Chuyển dãy byte thành số 32-bit (Big Endian)
  temp |= byte_array[0] << 24;
  temp |= byte_array[1] << 16;
  temp |= byte_array[2] << 8;
  temp |= byte_array[3]; // byte cuối cùng (Big Endian)

  // Sử dụng union để ép kiểu 32-bit sang float
  union {
    unsigned int i;
    float f;
  } data;

  data.i = temp; // Gán giá trị 32-bit cho i
  return data.f; // Trả về giá trị float
}

#if ph_fuvitech
unsigned char measured_command_ph_fuvitech[8] = {0x02, 0x03, 0x00, 0x01, 0x00, 0x02, 0x95, 0xF8};
unsigned char temperature_command_ph_fuvitech[8] = {0x02, 0x03, 0x00, 0x03, 0x00, 0x02, 0x34, 0x38};
#endif
#if ec_fuvitech
unsigned char conductivity_command_ec_fuvitech[8] = {0x03, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC5, 0xE9};
unsigned char resistivity_command_ec_fuvitech[8] = {0x03, 0x03, 0x00, 0x02, 0x00, 0x02, 0x64, 0x29};
unsigned char temperateure_command_ec_fuvitech[8] = {0x03, 0x03, 0x00, 0x04, 0x00, 0x02, 0x84, 0x28};
unsigned char tds_command_ec_fuvitech[8] = {0x03, 0x03, 0x00, 0x06, 0x00, 0x02, 0x25, 0xE8};
unsigned char salinity_command_ec_fuvitech[8] = {0x03, 0x03, 0x00, 0x08, 0x00, 0x02, 0x44, 0x2B};
#endif
#if do_fuvitech
unsigned char _command_do_fuvitech[8] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x2A, 0xC4, 0x15};
#endif

float read_ph_fuvitech(uint8_t *data, char * data_log) {
  printf("Read measured form %s\r\n",data_log);
  HAL_UART_Transmit(&huart4, data, 8, 1000);
  HAL_Delay(700);
  uint8_t reordered_data[4] = {rx_buffer_fuvitech[5], rx_buffer_fuvitech[6], rx_buffer_fuvitech[3], rx_buffer_fuvitech[4]};
  data_ph_fuvitech = ieee754_to_float(reordered_data);
  return data_ph_fuvitech;
}

float read_sensor_fuvitech(uint8_t *data, char * data_log) {
  printf("Read sensor %s\r\n",data_log);
  data_ec_fuvitech=0;
  HAL_UART_Transmit(&huart2, data, 8, 500);
  HAL_Delay(500);
  if(rx_buffer_fuvitech[0]==2&&rx_buffer_fuvitech[1]==3&&rx_buffer_fuvitech[2]==4){
	  uint8_t reordered_data[4] = {rx_buffer_fuvitech[5], rx_buffer_fuvitech[6], rx_buffer_fuvitech[3], rx_buffer_fuvitech[4]};
	  data_ec_fuvitech = ieee754_to_float(reordered_data);
  }
  if(rx_buffer_fuvitech[0]==3&&rx_buffer_fuvitech[1]==3&&rx_buffer_fuvitech[2]==4){
	  uint8_t reordered_data[4] = {rx_buffer_fuvitech[5], rx_buffer_fuvitech[6], rx_buffer_fuvitech[3], rx_buffer_fuvitech[4]};
	  data_ec_fuvitech = ieee754_to_float(reordered_data);
  }
  if(rx_buffer_fuvitech[0]==1&&rx_buffer_fuvitech[1]==3){
	  data_ec_fuvitech = (rx_buffer_fuvitech[43]<<8| rx_buffer_fuvitech[44]);
  }
  memset(rx_buffer_fuvitech, '\0', 100);
//  if(data_ec_fuvitech==0){
//	  HAL_GPIO_WritePin(GPIOA, ENABLE_SENSOR_Pin, GPIO_PIN_RESET);
//	  HAL_Delay(1000);
//	  HAL_GPIO_WritePin(GPIOA, ENABLE_SENSOR_Pin, GPIO_PIN_SET);
//	  HAL_Delay(500);
//  }
  return data_ec_fuvitech;
}

float read_do_fuvitech(uint8_t *data) {
  printf("Read measured form sensor EC\r\n");
  HAL_UART_Transmit(&huart2, data, 8, 1000);
  HAL_Delay(700);
  data_do_fuvitech = (rx_buffer_fuvitech[43]<<8| rx_buffer_fuvitech[44]);
  memset(rx_buffer_fuvitech, '\0', 100);
  return data_do_fuvitech;
}

void read_sensor(void) {
  // read PH Fuvitech
#if ph_fuvitech
  data_measured_ph_fuvitech = read_sensor_fuvitech(measured_command_ph_fuvitech,"PH");
  data_temperature_ph_fuvitech = read_sensor_fuvitech(temperature_command_ph_fuvitech,"PH");
#endif
  // read EC Fuvitech
#if ec_fuvitech
  data_conductivity_ec_fuvitech = (read_sensor_fuvitech(conductivity_command_ec_fuvitech,"EC")) * 1000;
  data_resistivity_ec_fuvitech = read_sensor_fuvitech(resistivity_command_ec_fuvitech,"EC");
  data_temperateure_ec_fuvitech = read_sensor_fuvitech(temperateure_command_ec_fuvitech,"EC");
  data_tds_ec_fuvitech = read_sensor_fuvitech(tds_command_ec_fuvitech,"EC");
  data_salinity_ec_fuvitech = (read_sensor_fuvitech(tds_command_ec_fuvitech,"EC")/640);
#endif
#if do_fuvitech
  data_dissolved_oxygen_fuvitech = read_sensor_fuvitech(_command_do_fuvitech,"DO")/100;
#endif
}
