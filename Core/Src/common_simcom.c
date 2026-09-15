/*
 * common_simcom.c
 *
 *  Created on: Mar 3, 2025
 *      Author: thuanphat
 */
#include "cJSON.h"
#include "config.h"
#include "stdbool.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include <main.h>
#include <stdbool.h>
#include "ph_pump_scheduler.h"
#include "ph_pump_isr.h"
#include "mobi_mqtt.h"

/* Private variables ---------------------------------------------------------*/
extern UART_HandleTypeDef huart1;
extern TIM_HandleTypeDef htim6;
extern TIM_HandleTypeDef htim3;
extern UART_HandleTypeDef huart5;
/* USER CODE BEGIN 0 */
char rx_data_sim[700];
char array_at_command[400];
char array_json[700];

char test_lcd[10]="hello";
ph_ctrl_cfg_t g_cfg_simcom;

int previousTick;
bool is_pb_done = false;
bool is_enable_mqtt = false;
bool is_connected_mqtt = false;
bool is_checked_sim = false;
bool is_subcribed_mqtt = false;
bool is_published_mqtt = false;
bool is_acquiered_mqtt = false;
bool is_updated_status = false;
uint8_t total_errors = 0;

bool is_at_connect_mqtt = false;
bool is_at_acquier_mqtt = false;
bool is_at_subcribe_topic_mqtt = false;
bool is_at_subcribe_mqtt = false;
bool is_at_topic_puplish_mqtt = false;
bool is_at_data_puplish_mqtt = false;
bool is_at_puplish_mqtt = false;
bool is_at_check_dis_mqtt = false;
bool is_at_disconnect_mqtt = false;
bool is_at_rel_mqtt = false;
bool is_at_stop_mqtt = false;
bool is_inital_check = false;
bool to_send_status_to_server = false;
bool to_start_ota = false;
//volatile bool control_mode;

uint16_t count_errors = 0;
int timeout_pb_done = 60000;
int is_connect_simcom = 0;
int rssi = -99;

uint16_t frequency_1hz = 0;


volatile uint8_t motor_ph_plus =false;
volatile uint8_t motor_ph_minus=false;
bool motor_x=false;

uint16_t frequency_1hz_timer2;

bool is_publish_data_lcd = false;

uint8_t check_sensor_ph_error=0;
uint8_t check_sensor_ec_error=0;
uint8_t check_sensor_do_error=0;
/* USER CODE END 0 */

//void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
//  if (htim->Instance == htim6.Instance) {
//    if (current_status_simcom == Off) {
//      frequency_1hz++;
//      if (frequency_1hz >= INTERVAL_PUPLISH_DATA) {
//        current_status_simcom = UpdateToServer;
//        to_send_status_to_server = 1;
//        frequency_1hz = 0;
//        data_measured_ph_fuvitech++;
//        printf("Case error log total :%d\r\n", total_errors);
//      }
//    }
////    HAL_TIM_Base_Start_IT(&htim6);
//  }
//  if (htim->Instance == htim3.Instance) {
//	  frequency_1hz_timer2++;
//  }
//
//}




void send_to_simcom_a76xx(char *cmd) {
  printf("STM32 Write: %s", cmd);
  HAL_UART_Transmit(&huart1, (uint8_t *)cmd, strlen(cmd), 1000);
}

void restart_stm32(void) {
  printf("\r\n-----------------Restart STM32------------------\r\n");
  printf("\r\n-----------------GOOD BYE !------------------\r\n");
#if SAVE_LOAD
  write_status_load();
#endif
  NVIC_SystemReset();
}

void enable_simcom(void) {
  printf("Enable SIMCOM\n");
#if SIMCOM_MODEL == a7080
  send_to_simcom_a76xx("AT+CPOWD=1\r\n");
  HAL_Delay(5000);
  HAL_GPIO_WritePin(PWRKEY_SIMCOM_GPIO_Port, PWRKEY_SIMCOM_Pin, GPIO_PIN_SET);
  HAL_Delay(4000);
  HAL_GPIO_WritePin(PWRKEY_SIMCOM_GPIO_Port, PWRKEY_SIMCOM_Pin, GPIO_PIN_RESET);
  HAL_Delay(4000);
#else
  HAL_GPIO_WritePin(GPIOA, ENABLE_SENSOR_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(PWRKEY_SIMCOM_GPIO_Port, PWRKEY_SIMCOM_Pin, GPIO_PIN_SET);
  HAL_Delay(3300);
  HAL_GPIO_WritePin(PWRKEY_SIMCOM_GPIO_Port, PWRKEY_SIMCOM_Pin, GPIO_PIN_RESET);
  HAL_Delay(3300);
  HAL_GPIO_WritePin(PWRKEY_SIMCOM_GPIO_Port, PWRKEY_SIMCOM_Pin, GPIO_PIN_SET);
  HAL_Delay(500);
  HAL_GPIO_WritePin(PWRKEY_SIMCOM_GPIO_Port, PWRKEY_SIMCOM_Pin, GPIO_PIN_RESET);
  HAL_Delay(1000);
#endif
}

bool wait_for_pb_done_event(void) {
  HAL_GPIO_WritePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin, RESET);
  previousTick = HAL_GetTick();
  while (is_inital_check == 0 && previousTick + timeout_pb_done > HAL_GetTick()) {
#if SIMCOM_MODEL == a7080
    send_to_simcom_a76xx("AT+CFGRI=1\r\n");
    HAL_Delay(200);
    send_to_simcom_a76xx("ATE0\r\n");
    HAL_Delay(200);
    send_to_simcom_a76xx("AT+CFUN?\r\n");
    HAL_Delay(200);
    send_to_simcom_a76xx("AT+CGDCONT?\r\n");
    HAL_Delay(200);
    send_to_simcom_a76xx("ATI\r\n");
    HAL_Delay(200);
    send_to_simcom_a76xx("AT+CGREG?\r\n");
    HAL_Delay(200);
    send_to_simcom_a76xx("AT+CBAND?\r\n");
    HAL_Delay(200);
    send_to_simcom_a76xx("ATE0\r\n");
    HAL_Delay(200);
    send_to_simcom_a76xx("AT+CCID\r\n");
    HAL_Delay(200);
    send_to_simcom_a76xx("AT+CNSMOD?\r\n");
    HAL_Delay(200);
    send_to_simcom_a76xx("ATI\r\n");
    HAL_Delay(200);
    send_to_simcom_a76xx("AT+CICCID\r\n");
    HAL_Delay(200);
    send_to_simcom_a76xx("AT+CMNB?\r\n");
    HAL_Delay(200);
    send_to_simcom_a76xx("AT+COPS=0\r\n");
    HAL_Delay(200);
    send_to_simcom_a76xx("AT+COPS?\r\n");
    HAL_Delay(200);
    read_signal_quality();
    HAL_Delay(200);
    send_to_simcom_a76xx("AT+CBANDCFG=\"NB-IOT\",1,3,4,5,8,13,18,19,20,25,26,28,66,71,85\r\n");
    HAL_Delay(200);
    send_to_simcom_a76xx("AT+CBANDCFG?\r\n");
    HAL_Delay(200);
    send_to_simcom_a76xx("AT+CGNAPN\r\n");
    HAL_Delay(200);
    send_to_simcom_a76xx("AT+CGATT?\r\n");
    HAL_Delay(500);
    return true;
#elif SIMCOM_MODEL == a7680
    HAL_Delay(12000);
    send_to_simcom_a76xx("ATE0\r\n");
    HAL_Delay(200);
    send_to_simcom_a76xx("AT+CCIOTOPTI?\r\n");
    HAL_Delay(200);
    return true;
//	}
#elif SIMCOM_MODEL == a7677
    if (strstr((char *)rx_data_sim, "EPS PDN ACT 1")) {
      is_pb_done = 1;
      led_status('B');
      HAL_Delay(8000);
      return true;
    }
#else
    {
      if (strstr((char *)rx_data_sim, "PB DONE")) {
        send_to_simcom_a76xx("ATE0\r\n");
        HAL_Delay(200);
        is_connect_simcom =1;
        return true;
      }
    }

#endif
  }
  if (is_connect_simcom == 0) {
    NVIC_SystemReset();
  }
  return false;
}

int read_signal_quality(void) {
  send_to_simcom_a76xx("AT+CSQ\r\n");
  HAL_Delay(200);
  int signal, error;
  char *ptr = strstr(rx_data_sim, "+CSQ:");
  if (ptr && sscanf(ptr, "+CSQ: %d,%d", &signal, &error) == 2) {
    printf("Cuong do tin hieu: %d\n", signal);
    printf("Gia tri loi: %d\n", error);
  } else {
    printf("khong the phan tich chuoi @@ !\n");
  }
  if (signal >= 31) {
    rssi = -51;
  } else if (signal <= 0) {
    printf("not known or not detectable !\r\n");
    rssi = -113;
  } else if (signal == 99) {
    printf("not known or not detectable !\r\n");
    rssi = -113;
  } else
    rssi = (signal * 2 - 113);
  return rssi;
}

bool check_signal_simcom(void) {
  printf("-----------------check_signal_simcom------------------\n");
  read_signal_quality();
  HAL_Delay(200);
  is_connect_simcom = 1;
  HAL_Delay(200);
  send_to_simcom_a76xx("AT+CPIN?\r\n");
  HAL_Delay(200);
  if (strstr((char *)rx_data_sim, "+CPIN: READY")) {
    printf("-----------------SIM OK !------------------\n");
  } else
    return false;
  HAL_Delay(200);
  send_to_simcom_a76xx("AT+CGREG?\r\n");
  HAL_Delay(200);
  send_to_simcom_a76xx("AT+CREG?\r\n");
  HAL_Delay(200);
  send_to_simcom_a76xx("ATI\r\n");
  HAL_Delay(200);
  send_to_simcom_a76xx("AT+CICCID\r\n");
  HAL_Delay(200);
  send_to_simcom_a76xx("AT+CGREG?\r\n");
  HAL_Delay(400);
  if (strstr((char *)rx_data_sim, "+CGREG: 0,1")) {
    printf("-----------------Network registration OK!------------------\n");
  } else
    return false;
  return true;
}

bool enable_mqtt_on_gsm_modem(void) {
  is_enable_mqtt = mobi_mqtt_start();
#if SIMCOM_MODEL == a7080
  is_connected_mqtt = is_enable_mqtt;
#endif
  return is_enable_mqtt;
}

bool acquire_gsm_mqtt_client(void) {
  is_acquiered_mqtt = mobi_mqtt_acquire_client();
  return is_acquiered_mqtt;
}

bool connect_mqtt_server_by_gsm(void) {
  is_connected_mqtt = mobi_mqtt_connect();
  return is_connected_mqtt;
}

bool subscribe_mqtt_via_gsm(void) {
  is_subcribed_mqtt = mobi_mqtt_subscribe_server_topics();
  if (is_subcribed_mqtt) {
    HAL_GPIO_WritePin(LED_STATUS_GPIO_Port, LED_STATUS_Pin, SET);
#if SENSOR_DATA_SOURCE == SENSOR_SOURCE_DIRECT
    read_sensor();
#endif
    mobi_mqtt_after_subscribe_online();
  }
  return is_subcribed_mqtt;
}

void create_JSON(void) {
  cJSON *json = cJSON_CreateObject();
  rssi = read_signal_quality();
  data_percentage_pin = read_level_pin();
  cJSON_AddNumberToObject(json, "_gsm_signal_strength", rssi);
  cJSON_AddNumberToObject(json, "_battery_level", data_percentage_pin);
  cJSON_AddNumberToObject(json, "control_mode", g_control_mode);


#if ph_fuvitech
  // data PH Fuvitech
//  if(data_measured_ph_fuvitech<1){
//      check_sensor_ph_error++;
//  }
//  else{
//      check_sensor_ph_error=0;
//  }
//  if(check_sensor_ph_error>=3){
//      NVIC_SystemReset();
//  }

  char data_measured_ph_fuvitech_str[16];
  char data_temperature_ph_fuvitech_str[16];
  snprintf(data_measured_ph_fuvitech_str, sizeof(data_measured_ph_fuvitech_str), "%.2f", data_measured_ph_fuvitech);
  snprintf(data_temperature_ph_fuvitech_str, sizeof(data_temperature_ph_fuvitech_str), "%.2f", data_temperature_ph_fuvitech);
  cJSON_AddStringToObject(json, "solPH", data_measured_ph_fuvitech_str);
  cJSON_AddStringToObject(json, "solT", data_temperature_ph_fuvitech_str);
#endif


#if ph_rika500_12
  // data PH Fuvitech
//  if(data_measured_ph_fuvitech<1){
//      check_sensor_ph_error++;
//  }
//  else{
//      check_sensor_ph_error=0;
//  }
//  if(check_sensor_ph_error>=3){
//      NVIC_SystemReset();
//  }

  char data_measured_ph_fuvitech_str[16];
  char data_temperature_ph_fuvitech_str[16];
  snprintf(data_measured_ph_fuvitech_str, sizeof(data_measured_ph_fuvitech_str), "%.2f", data_measured_ph_fuvitech);
  snprintf(data_temperature_ph_fuvitech_str, sizeof(data_temperature_ph_fuvitech_str), "%.2f", data_temperature_ph_fuvitech);
  cJSON_AddStringToObject(json, "solPH", data_measured_ph_fuvitech_str);
  cJSON_AddStringToObject(json, "solT", data_temperature_ph_fuvitech_str);
#endif
#if ec_fuvitech
  if(data_conductivity_ec_fuvitech<1){
      check_sensor_ec_error++;
  }
  else{
      check_sensor_ec_error=0;
  }
  if(check_sensor_ec_error>=3){
      NVIC_SystemReset();
  }
  char data_conductivity_ec_fuvitech_str[16];
  char data_tds_ec_fuvitech_str[16];
  char data_resistivity_ec_fuvitech_str[16];
  char data_salinity_ec_fuvitech_str[16];
  char data_temperature_ec_fuvitech_str[16];
  snprintf(data_conductivity_ec_fuvitech_str, sizeof(data_conductivity_ec_fuvitech_str), "%.2f", data_conductivity_ec_fuvitech);
  snprintf(data_tds_ec_fuvitech_str, sizeof(data_tds_ec_fuvitech_str), "%.2f", data_tds_ec_fuvitech);
  snprintf(data_resistivity_ec_fuvitech_str, sizeof(data_resistivity_ec_fuvitech_str), "%.2f", data_resistivity_ec_fuvitech);
  snprintf(data_salinity_ec_fuvitech_str, sizeof(data_salinity_ec_fuvitech_str), "%.2f", data_salinity_ec_fuvitech);
  snprintf(data_temperature_ec_fuvitech_str, sizeof(data_temperature_ec_fuvitech_str), "%.2f", data_temperateure_ec_fuvitech);
  //   data EC Fuvitech
  cJSON_AddStringToObject(json, "solEC", data_conductivity_ec_fuvitech_str);
  cJSON_AddStringToObject(json, "solTDS", data_tds_ec_fuvitech_str);
  cJSON_AddStringToObject(json, "solRes", data_resistivity_ec_fuvitech_str);
  cJSON_AddStringToObject(json, "solSal", data_salinity_ec_fuvitech_str);
  cJSON_AddStringToObject(json, "solT", data_temperature_ec_fuvitech_str);
#endif
#if ec_rika500_13
//  if(data_conductivity_ec_fuvitech<1){
//      check_sensor_ec_error++;
//  }
//  else{
//      check_sensor_ec_error=0;
//  }
//  if(check_sensor_ec_error>=3){
//      NVIC_SystemReset();
//  }
  char data_conductivity_ec_fuvitech_str[16];
  char data_resistivity_ec_fuvitech_str[16];
  char data_temperature_ec_fuvitech_str[16];
  char data_tds_ec_fuvitech_str[16];
  char data_salinity_ec_fuvitech_str[16];
  snprintf(data_conductivity_ec_fuvitech_str, sizeof(data_conductivity_ec_fuvitech_str), "%.2f", data_conductivity_ec_fuvitech);
  snprintf(data_resistivity_ec_fuvitech_str, sizeof(data_resistivity_ec_fuvitech_str), "%.2f", data_resistivity_ec_fuvitech);
  snprintf(data_temperature_ec_fuvitech_str, sizeof(data_temperature_ec_fuvitech_str), "%.2f", data_temperateure_ec_fuvitech);
  snprintf(data_tds_ec_fuvitech_str, sizeof(data_tds_ec_fuvitech_str), "%.2f", data_tds_ec_fuvitech);
  snprintf(data_salinity_ec_fuvitech_str, sizeof(data_salinity_ec_fuvitech_str), "%.2f", data_salinity_ec_fuvitech);
  //   data EC Fuvitech
  cJSON_AddStringToObject(json, "solEC", data_conductivity_ec_fuvitech_str);
  cJSON_AddStringToObject(json, "solRes", data_resistivity_ec_fuvitech_str);
  cJSON_AddStringToObject(json, "solTDS", data_tds_ec_fuvitech_str);
  cJSON_AddStringToObject(json, "solSal", data_salinity_ec_fuvitech_str);
//  cJSON_AddStringToObject(json, "solT", data_temperature_ec_fuvitech_str);
#endif
#if do_fuvitech
  //   data DO Fuvitech
//  if(data_dissolved_oxygen_fuvitech<1){
//      check_sensor_do_error++;
//  }
//  else{
//      check_sensor_do_error=0;
//  }
//  if(check_sensor_do_error>=3){
//      NVIC_SystemReset();
//  }
  char data_dissolved_oxygen_str[16];
  snprintf(data_dissolved_oxygen_str, sizeof(data_dissolved_oxygen_str), "%.2f", data_dissolved_oxygen_fuvitech);
  cJSON_AddStringToObject(json, "solDO", data_dissolved_oxygen_str);
#endif

#if do_rika500_04

  char data_dissolved_oxygen_str[16];
  snprintf(data_dissolved_oxygen_str, sizeof(data_dissolved_oxygen_str), "%.2f", data_dissolved_oxygen_rika);
  cJSON_AddStringToObject(json, "solDO", data_dissolved_oxygen_str);
#endif

  char *json_string = cJSON_PrintUnformatted(json);
  if (json_string == NULL) {
    printf("New create error JSON\n");
    cJSON_Delete(json);
    return;
  }
  snprintf(array_json, sizeof(array_json), "%s", json_string);
  // decompress memory
  free(json_string);
  cJSON_Delete(json);
}
void create_JSON_LCD(void) {
#if SENSOR_DATA_SOURCE == SENSOR_SOURCE_PLC_RS485
  mobi_mqtt_build_telemetry_json(false);
  return;
#else
  cJSON *json = cJSON_CreateObject();
  data_percentage_pin = read_level_pin();
  cJSON_AddNumberToObject(json, "_gsm_signal_strength", rssi);
  cJSON_AddNumberToObject(json, "_battery_level", data_percentage_pin);
  cJSON_AddNumberToObject(json, "control_mode", g_control_mode);


#if ph_fuvitech
  // data PH Fuvitech
//  if(data_measured_ph_fuvitech<1){
//      check_sensor_ph_error++;
//  }
//  else{
//      check_sensor_ph_error=0;
//  }
//  if(check_sensor_ph_error>=3){
//      NVIC_SystemReset();
//  }

  char data_measured_ph_fuvitech_str[16];
  char data_temperature_ph_fuvitech_str[16];
  snprintf(data_measured_ph_fuvitech_str, sizeof(data_measured_ph_fuvitech_str), "%.2f", data_measured_ph_fuvitech);
  snprintf(data_temperature_ph_fuvitech_str, sizeof(data_temperature_ph_fuvitech_str), "%.2f", data_temperature_ph_fuvitech);
  cJSON_AddStringToObject(json, "solPH", data_measured_ph_fuvitech_str);
  cJSON_AddStringToObject(json, "solT", data_temperature_ph_fuvitech_str);
#endif

#if ph_rika500_12
  // data PH Fuvitech
//  if(data_measured_ph_fuvitech<1){
//      check_sensor_ph_error++;
//  }
//  else{
//      check_sensor_ph_error=0;
//  }
//  if(check_sensor_ph_error>=3){
//      NVIC_SystemReset();
//  }

  char data_measured_ph_fuvitech_str[16];
  char data_temperature_ph_fuvitech_str[16];
  snprintf(data_measured_ph_fuvitech_str, sizeof(data_measured_ph_fuvitech_str), "%.2f", data_measured_ph_fuvitech);
  snprintf(data_temperature_ph_fuvitech_str, sizeof(data_temperature_ph_fuvitech_str), "%.2f", data_temperature_ph_fuvitech);
  cJSON_AddStringToObject(json, "solPH", data_measured_ph_fuvitech_str);
  cJSON_AddStringToObject(json, "solT", data_temperature_ph_fuvitech_str);
#endif

#if ec_fuvitech
  if(data_conductivity_ec_fuvitech<1){
      check_sensor_ec_error++;
  }
  else{
      check_sensor_ec_error=0;
  }
  if(check_sensor_ec_error>=3){
      NVIC_SystemReset();
  }
  char data_conductivity_ec_fuvitech_str[16];
  char data_tds_ec_fuvitech_str[16];
  char data_resistivity_ec_fuvitech_str[16];
  char data_salinity_ec_fuvitech_str[16];
  char data_temperature_ec_fuvitech_str[16];
  snprintf(data_conductivity_ec_fuvitech_str, sizeof(data_conductivity_ec_fuvitech_str), "%.2f", data_conductivity_ec_fuvitech);
  snprintf(data_tds_ec_fuvitech_str, sizeof(data_tds_ec_fuvitech_str), "%.2f", data_tds_ec_fuvitech);
  snprintf(data_resistivity_ec_fuvitech_str, sizeof(data_resistivity_ec_fuvitech_str), "%.2f", data_resistivity_ec_fuvitech);
  snprintf(data_salinity_ec_fuvitech_str, sizeof(data_salinity_ec_fuvitech_str), "%.2f", data_salinity_ec_fuvitech);
  snprintf(data_temperature_ec_fuvitech_str, sizeof(data_temperature_ec_fuvitech_str), "%.2f", data_temperateure_ec_fuvitech);
  //   data EC Fuvitech
  cJSON_AddStringToObject(json, "solEC", data_conductivity_ec_fuvitech_str);
  cJSON_AddStringToObject(json, "solTDS", data_tds_ec_fuvitech_str);
  cJSON_AddStringToObject(json, "solRes", data_resistivity_ec_fuvitech_str);
  cJSON_AddStringToObject(json, "solSal", data_salinity_ec_fuvitech_str);
  cJSON_AddStringToObject(json, "solT", data_temperature_ec_fuvitech_str);
#endif
#if ec_rika500_13
//  if(data_conductivity_ec_fuvitech<1){
//      check_sensor_ec_error++;
//  }
//  else{
//      check_sensor_ec_error=0;
//  }
//  if(check_sensor_ec_error>=3){
//      NVIC_SystemReset();
//  }
  char data_conductivity_ec_fuvitech_str[16];
  char data_resistivity_ec_fuvitech_str[16];
  char data_temperature_ec_fuvitech_str[16];
  char data_tds_ec_fuvitech_str[16];
  char data_salinity_ec_fuvitech_str[16];
  snprintf(data_conductivity_ec_fuvitech_str, sizeof(data_conductivity_ec_fuvitech_str), "%.2f", data_conductivity_ec_fuvitech);
  snprintf(data_resistivity_ec_fuvitech_str, sizeof(data_resistivity_ec_fuvitech_str), "%.2f", data_resistivity_ec_fuvitech);
  snprintf(data_temperature_ec_fuvitech_str, sizeof(data_temperature_ec_fuvitech_str), "%.2f", data_temperateure_ec_fuvitech);
  snprintf(data_tds_ec_fuvitech_str, sizeof(data_tds_ec_fuvitech_str), "%.2f", data_tds_ec_fuvitech);
  snprintf(data_salinity_ec_fuvitech_str, sizeof(data_salinity_ec_fuvitech_str), "%.2f", data_salinity_ec_fuvitech);
  //   data EC Fuvitech
  cJSON_AddStringToObject(json, "solEC", data_conductivity_ec_fuvitech_str);
  cJSON_AddStringToObject(json, "solRes", data_resistivity_ec_fuvitech_str);
  cJSON_AddStringToObject(json, "solTDS", data_tds_ec_fuvitech_str);
  cJSON_AddStringToObject(json, "solSal", data_salinity_ec_fuvitech_str);
//  cJSON_AddStringToObject(json, "solT", data_temperature_ec_fuvitech_str);
#endif
#if do_fuvitech
  //   data DO Fuvitech
//  if(data_dissolved_oxygen_fuvitech<1){
//      check_sensor_do_error++;
//  }
//  else{
//      check_sensor_do_error=0;
//  }
//  if(check_sensor_do_error>=3){
//      NVIC_SystemReset();
//  }
  char data_dissolved_oxygen_str[16];
  snprintf(data_dissolved_oxygen_str, sizeof(data_dissolved_oxygen_str), "%.2f", data_dissolved_oxygen_fuvitech);
  cJSON_AddStringToObject(json, "solDO", data_dissolved_oxygen_str);
#endif


#if do_rika500_04

  char data_dissolved_oxygen_str[16];
  snprintf(data_dissolved_oxygen_str, sizeof(data_dissolved_oxygen_str), "%.2f", data_dissolved_oxygen_rika);
  cJSON_AddStringToObject(json, "solDO", data_dissolved_oxygen_str);
#endif

  char *json_string = cJSON_PrintUnformatted(json);
  if (json_string == NULL) {
    printf("New create error JSON\n");
    cJSON_Delete(json);
    return;
  }
  snprintf(array_json, sizeof(array_json), "%s", json_string);
  // decompress memory
  free(json_string);
  cJSON_Delete(json);
#endif
}

void create_Json_status_motor(void)
{
	  cJSON *json = cJSON_CreateObject();
	  cJSON_AddNumberToObject(json, "1", motor_ph_plus);
	  cJSON_AddNumberToObject(json, "2", motor_ph_minus);
	  cJSON_AddNumberToObject(json, "3", motor_x);

	  char *json_string = cJSON_PrintUnformatted(json);
	  if (json_string == NULL) {
	    printf("New create error JSON\n");
	    cJSON_Delete(json);
	    return;
	  }
	  snprintf(array_json, sizeof(array_json), "%s", json_string);
	  // decompress memory
	  free(json_string);
	  cJSON_Delete(json);
}
bool publish_mqtt_via_gsm(void) {
  is_at_puplish_mqtt = mobi_mqtt_publish_telemetry();
  return is_at_puplish_mqtt;
}

bool publish_mqtt_motor_status (void)
{
  is_at_puplish_mqtt = mobi_mqtt_publish_config_state();
  return is_at_puplish_mqtt;
}
bool stop_mqtt_via_gsm(void) {
  return mobi_mqtt_disconnect();
}

bool send_payload_signal_to_server(void) {
  for (int i = 1; i <= 3; i++) {
    is_published_mqtt = publish_mqtt_via_gsm();
    if (is_published_mqtt) {
      return true;
    }
  }
  if (!is_published_mqtt) {
    return false;
  }
  return false;
}

void sleep_stm32(void) {
  printf("begin sleep mode STM32");
  HAL_GPIO_WritePin(PWRKEY_SIMCOM_GPIO_Port, PWRKEY_SIMCOM_Pin, GPIO_PIN_SET);
  HAL_Delay(3000);
  HAL_GPIO_WritePin(PWRKEY_SIMCOM_GPIO_Port, PWRKEY_SIMCOM_Pin, GPIO_PIN_RESET);
  HAL_Delay(6000);
  HAL_GPIO_WritePin(GPIOB, LED_STATUS_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOA, ENABLE_SENSOR_Pin, GPIO_PIN_RESET);
  printf("--------GOOD BYE !-------");
  HAL_TIM_Base_Stop_IT(&htim6);
    HAL_ADC_Stop_DMA(&hadc1);
  HAL_TIM_Base_Start_IT(&htim3);
  HAL_SuspendTick();
  HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFE);
  HAL_ResumeTick();
  NVIC_SystemReset();
}

bool update_data_to_sreen(uint8_t *data) {
  size_t len;
  uint32_t timeout_ms;
  HAL_StatusTypeDef status;

  if (data == NULL) {
    return false;
  }

  len = strlen((const char *)data);
  if (len == 0U) {
    return false;
  }

  printf("update data to screen len=%u\r\n", (unsigned int)len);

  timeout_ms = (uint32_t)(len * 3U) + 500U;
  status = HAL_UART_Transmit(&huart5, data, (uint16_t)len, timeout_ms);
  if (status != HAL_OK) {
    printf("update data to screen fail status=%d\r\n", status);
    return false;
  }

  if (len < 2U || data[len - 2U] != '\r' || data[len - 1U] != '\n') {
    uint8_t newline[] = "\r\n";
    status = HAL_UART_Transmit(&huart5, newline, 2U, 100U);
    if (status != HAL_OK) {
      printf("update data to screen newline fail status=%d\r\n", status);
      return false;
    }
  }

  return true;
}

void check_handle_state(enum GmsModemState status) {
  switch (status) {
  case Off: {
    enable_simcom();
    IWDG->KR = 0xAAAA;
    is_pb_done = wait_for_pb_done_event();
    if (is_pb_done) {
      current_status_simcom = On;
      printf("Current status SIMCOM On \r\n");
    } else {
      NVIC_SystemReset();
    }
    break;
  }
  case On: {
    IWDG->KR = 0xAAAA;
    is_checked_sim = check_signal_simcom();
    if (is_checked_sim) {
      total_errors = 0;
      current_status_simcom = InternetReady;
      printf("SIMCOM current status is Internet ready\r\n");
    } else {
      total_errors++;
    }
    if (total_errors > 10) {
      restart_stm32();
    }
    break;
  }
  case InternetReady: {
    IWDG->KR = 0xAAAA;
    is_enable_mqtt = enable_mqtt_on_gsm_modem();
#if SIMCOM_MODEL != a7080
    if (is_enable_mqtt) {
      is_acquiered_mqtt = acquire_gsm_mqtt_client();
    } else {
      NVIC_SystemReset();
    }
    if (is_acquiered_mqtt) {
      is_connected_mqtt = connect_mqtt_server_by_gsm();
    }
#endif
    if (is_connected_mqtt) {
      total_errors = 0;
      current_status_simcom = MqttReady;
      printf("SIMCOM current status is MQTT ready\r\n");
    } else
      total_errors++;
    if (total_errors > 3) {
      restart_stm32();
    }
    break;
  }
  case MqttReady: {
	IWDG->KR = 0xAAAA;
    is_subcribed_mqtt = subscribe_mqtt_via_gsm();
    if (is_subcribed_mqtt) {
      total_errors = 0;
      current_status_simcom = Subscribed;
      printf("Current status Simcom subscribed \r\n");
    } else
      total_errors++;
    if (total_errors > 5) {
      restart_stm32();
    }
    break;
  }
  case Subscribed: {
#if INTERVAL_PUPLISH_DATA < 60
	  if (to_send_status_to_server) {
#if SENSOR_DATA_SOURCE == SENSOR_SOURCE_DIRECT
      read_sensor();
#endif
		  	//process_uart_rx();
      printf("Publish telemetry by interval\r\n");
      IWDG->KR = 0xAAAA;
      is_updated_status = send_payload_signal_to_server();
//      is_publish_data_lcd = update_data_to_sreen((uint8_t *)array_json);

      if (is_updated_status) {
        to_send_status_to_server = 0;
        IWDG->KR = 0xAAAA;
        total_errors = 0;
      } else {
        total_errors++;
        if (total_errors > 5) {
          stop_mqtt_via_gsm();
          current_status_simcom = On;
        }
      }
  }

#else
#if SENSOR_DATA_SOURCE == SENSOR_SOURCE_DIRECT
      read_sensor();
#endif
      is_updated_status = send_payload_signal_to_server();
      if (is_updated_status) {
        to_send_status_to_server = 0;
        IWDG->KR = 0xAAAA;
        total_errors = 0;
    current_status_simcom = SleepStm32;
      } else {
        total_errors++;
        if (total_errors > 3) {
          stop_mqtt_via_gsm();
          current_status_simcom = On;
        }
      }

#endif
	  break;
  }

  case ControlModeOffline: {

	  break;
  }
#if INTERVAL_PUPLISH_DATA >= 60
  case SleepStm32: {
	  sleep_stm32();
	  break;
  }
#endif
  default:
    printf("Case cannot be determined !\r\n");
  }
}
