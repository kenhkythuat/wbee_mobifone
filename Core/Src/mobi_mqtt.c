/*
 * mobi_mqtt.c
 *
 * MQTT flow for Mobi water monitoring protocol v1.0.
 */
#include "mobi_mqtt.h"

#include "cJSON.h"
#include "config.h"
#include "main.h"
#include "plc_rs485.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

extern char rx_data_sim[700];
extern char array_at_command[400];
extern char array_json[700];
extern int rssi;

extern void send_to_simcom_a76xx(char *cmd);
extern int read_signal_quality(void);

static void simcom_clear_rx(void) {
  memset(rx_data_sim, '\0', sizeof(rx_data_sim));
}

static bool simcom_response_has(const char *text) {
  return strstr((char *)rx_data_sim, text) != NULL;
}

static bool simcom_response_ok(void) {
  return simcom_response_has("OK");
}

static bool simcom_wait_for(const char *text, uint32_t timeout_ms) {
  uint32_t start_tick = HAL_GetTick();
  while ((HAL_GetTick() - start_tick) < timeout_ms) {
    if (simcom_response_has(text)) {
      return true;
    }
    HAL_Delay(50);
  }
  return false;
}

static bool simcom_wait_for_ok(uint32_t timeout_ms) {
  return simcom_wait_for("OK", timeout_ms);
}

static bool simcom_wait_for_ok_or(const char *text, uint32_t timeout_ms) {
  uint32_t start_tick = HAL_GetTick();
  while ((HAL_GetTick() - start_tick) < timeout_ms) {
    if (simcom_response_ok() || simcom_response_has(text)) {
      return true;
    }
    HAL_Delay(50);
  }
  return false;
}

static void simcom_log_response(void) {
  printf("SIMCOM Response: %s\r\n", rx_data_sim);
}

static bool json_to_array(cJSON *json) {
  char *json_string = cJSON_PrintUnformatted(json);
  if (json_string == NULL) {
    return false;
  }

  snprintf(array_json, sizeof(array_json), "%s", json_string);
  free(json_string);
  return true;
}

static void add_nullable_number(cJSON *json, const char *name, bool has_value,
                                double value) {
  if (has_value) {
    char raw_number[24];
    snprintf(raw_number, sizeof(raw_number), "%.2f", value);
    for (int i = (int)strlen(raw_number) - 1; i > 0; i--) {
      if (raw_number[i] == '0') {
        raw_number[i] = '\0';
      } else if (raw_number[i] == '.') {
        raw_number[i + 2] = '\0';
        raw_number[i + 1] = '0';
        break;
      } else {
        break;
      }
    }
    cJSON_AddRawToObject(json, name, raw_number);
  } else {
    cJSON_AddNullToObject(json, name);
  }
}

static void add_nullable_uint(cJSON *json, const char *name, bool has_value,
                              uint8_t value) {
  if (has_value) {
    cJSON_AddNumberToObject(json, name, value);
  } else {
    cJSON_AddNullToObject(json, name);
  }
}

static void build_status_payload(const char *status) {
  cJSON *json = cJSON_CreateObject();
  cJSON_AddStringToObject(json, "device_id", SERIAL_NUMBER);
  cJSON_AddStringToObject(json, "status", status);
  if (strcmp(status, "online") == 0) {
    cJSON_AddStringToObject(json, "firmware_version", VERSION_WBEE);
    rssi = read_signal_quality();
    cJSON_AddNumberToObject(json, "rssi", rssi);
  }
  json_to_array(json);
  cJSON_Delete(json);
}

static void build_config_state_payload(void) {
  cJSON *json = cJSON_CreateObject();
  cJSON_AddStringToObject(json, "device_id", SERIAL_NUMBER);
  cJSON_AddNumberToObject(json, "telemetry_interval_s",
                          TELEMETRY_INTERVAL_DEFAULT_S);
  cJSON_AddNumberToObject(json, "sensor_sample_interval_s",
                          SENSOR_SAMPLE_INTERVAL_DEFAULT_S);
  json_to_array(json);
  cJSON_Delete(json);
}

void mobi_mqtt_build_telemetry_json(bool refresh_rssi) {
  cJSON *json = cJSON_CreateObject();
  cJSON_AddStringToObject(json, "device_id", SERIAL_NUMBER);
  cJSON_AddStringToObject(json, "firmware_version", VERSION_WBEE);

#if SENSOR_DATA_SOURCE == SENSOR_SOURCE_PLC_RS485
  plc_rs485_data_t plc_data;
  plc_rs485_get_data(&plc_data);
  add_nullable_number(json, "ph1", plc_data.has_ph1, plc_data.ph1);
  add_nullable_number(json, "do", plc_data.has_dissolved_oxygen,
                      plc_data.dissolved_oxygen);
  add_nullable_number(json, "ph2", plc_data.has_ph2, plc_data.ph2);
  add_nullable_number(json, "turbidity", plc_data.has_turbidity,
                      plc_data.turbidity);
  add_nullable_number(json, "ozone", plc_data.has_ozone, plc_data.ozone);
  add_nullable_number(json, "pressure_o2", plc_data.has_pressure,
                      plc_data.pressure);
  add_nullable_number(json, "temp_data_1", plc_data.has_temp_data_1,
                      plc_data.temp_data_1);
  add_nullable_number(json, "temp_data_2", plc_data.has_temp_data_2,
                      plc_data.temp_data_2);
  add_nullable_number(json, "temp_data_3", plc_data.has_temp_data_3,
                      plc_data.temp_data_3);
  add_nullable_uint(json, "input_x", plc_data.has_input_x, plc_data.input_x);
  add_nullable_uint(json, "output_1", plc_data.has_output_1,
                    plc_data.output_1);
  add_nullable_uint(json, "output_2", plc_data.has_output_2,
                    plc_data.output_2);
  add_nullable_uint(json, "error_code", plc_data.has_error_code,
                    plc_data.error_code);
#else
  bool has_ph1 = false;
  bool has_do = false;
  double ph1_value = 0.0;
  double do_value = 0.0;

#if ph_fuvitech || ph_rika500_12
  has_ph1 = data_measured_ph_fuvitech > 0.0f;
  ph1_value = data_measured_ph_fuvitech;
#endif

#if do_fuvitech
  has_do = data_dissolved_oxygen_fuvitech > 0.0f;
  do_value = data_dissolved_oxygen_fuvitech;
#elif do_rika500_04
  has_do = data_dissolved_oxygen_rika > 0.0f;
  do_value = data_dissolved_oxygen_rika;
#endif

  add_nullable_number(json, "ph1", has_ph1, ph1_value);
  add_nullable_number(json, "do", has_do, do_value);
  add_nullable_number(json, "ph2", false, 0.0);
  add_nullable_number(json, "turbidity", false, 0.0);
  add_nullable_number(json, "ozone", false, 0.0);
  add_nullable_number(json, "pressure_o2", false, 0.0);
  add_nullable_number(json, "temp_data_1", false, 0.0);
  add_nullable_number(json, "temp_data_2", false, 0.0);
  add_nullable_number(json, "temp_data_3", false, 0.0);
  add_nullable_uint(json, "input_x", false, 0);
  add_nullable_uint(json, "output_1", false, 0);
  add_nullable_uint(json, "output_2", false, 0);
  add_nullable_uint(json, "error_code", false, 0);
#endif

  if (refresh_rssi) {
    rssi = read_signal_quality();
  }
  cJSON_AddNumberToObject(json, "rssi", rssi);
  json_to_array(json);
  cJSON_Delete(json);
}

#if SIMCOM_MODEL == a7080
static bool mqtt_publish_raw(const char *topic, const char *payload, uint8_t qos,
                             uint8_t retain) {
  simcom_clear_rx();
  send_to_simcom_a76xx("AT+SMSTATE?\r\n");
  simcom_wait_for_ok(1000);
  if (!simcom_response_has("+SMSTATE: 1")) {
    printf("-----------------MQTT offline------------------\n");
    simcom_log_response();
    return false;
  }

  snprintf(array_at_command, sizeof(array_at_command),
           "AT+SMPUB=\"%s\",%d,%d,%d\r\n", topic, (int)strlen(payload), qos,
           retain);
  simcom_clear_rx();
  send_to_simcom_a76xx(array_at_command);
  HAL_Delay(300);

  snprintf(array_at_command, sizeof(array_at_command), "%s\r\n", payload);
  send_to_simcom_a76xx(array_at_command);
  simcom_wait_for_ok(2000);

  if (simcom_response_ok()) {
    printf("-----------------Publish Success------------------\n");
    return true;
  }

  printf("-----------------Publish fail------------------\n");
  return false;
}
#else
static bool mqtt_publish_raw(const char *topic, const char *payload, uint8_t qos,
                             uint8_t retain) {
  snprintf(array_at_command, sizeof(array_at_command),
           "AT+CMQTTTOPIC=0,%d\r\n", (int)strlen(topic));
  simcom_clear_rx();
  send_to_simcom_a76xx(array_at_command);
  HAL_Delay(500);

  snprintf(array_at_command, sizeof(array_at_command), "%s\r\n", topic);
  send_to_simcom_a76xx(array_at_command);
  simcom_wait_for_ok(2000);
  if (!simcom_response_ok()) {
    printf("-----------------MQTT topic input fail------------------\n");
    simcom_log_response();
    return false;
  }

  snprintf(array_at_command, sizeof(array_at_command),
           "AT+CMQTTPAYLOAD=0,%d\r\n", (int)strlen(payload));
  simcom_clear_rx();
  send_to_simcom_a76xx(array_at_command);
  HAL_Delay(500);

  send_to_simcom_a76xx((char *)payload);
  simcom_wait_for_ok(2000);
  if (!simcom_response_ok()) {
    printf("-----------------MQTT payload input fail------------------\n");
    simcom_log_response();
    return false;
  }

  snprintf(array_at_command, sizeof(array_at_command),
           "AT+CMQTTPUB=0,%d,%d,%d\r\n", qos, MQTT_KEEPALIVE_SEC, retain);
  simcom_clear_rx();
  send_to_simcom_a76xx(array_at_command);
  simcom_wait_for("+CMQTTPUB: 0,0", 5000);
  if (simcom_response_has("+CMQTTPUB: 0,0")) {
    printf("-----------------Publish Success------------------\n");
    return true;
  }

  printf("-----------------Publish fail------------------\n");
  simcom_log_response();
  return false;
}
#endif

static bool mqtt_subscribe_raw(const char *topic) {
#if SIMCOM_MODEL == a7080
  snprintf(array_at_command, sizeof(array_at_command), "AT+SMSUB=\"%s\",%d\r\n",
           topic, MQTT_QOS);
  simcom_clear_rx();
  send_to_simcom_a76xx(array_at_command);
  simcom_wait_for_ok(5000);
  return simcom_response_ok();
#else
  snprintf(array_at_command, sizeof(array_at_command),
           "AT+CMQTTSUBTOPIC=0,%d,%d\r\n", (int)strlen(topic), MQTT_QOS);
  simcom_clear_rx();
  send_to_simcom_a76xx(array_at_command);
  HAL_Delay(500);

  snprintf(array_at_command, sizeof(array_at_command), "%s\r\n", topic);
  send_to_simcom_a76xx(array_at_command);
  simcom_wait_for_ok(2000);
  if (!simcom_response_ok()) {
    return false;
  }

  simcom_clear_rx();
  send_to_simcom_a76xx("AT+CMQTTSUB=0\r\n");
  simcom_wait_for("+CMQTTSUB: 0,0", 5000);
  return simcom_response_has("+CMQTTSUB: 0,0");
#endif
}

static bool mqtt_configure_last_will(void) {
  build_status_payload("offline");
#if SIMCOM_MODEL == a7080
  snprintf(array_at_command, sizeof(array_at_command),
           "AT+SMCONF=\"WILLTOPIC\",\"%s\"\r\n", MQTT_TOPIC_STATUS);
  send_to_simcom_a76xx(array_at_command);
  HAL_Delay(300);
  snprintf(array_at_command, sizeof(array_at_command),
           "AT+SMCONF=\"WILLMSG\",\"%s\"\r\n", array_json);
  send_to_simcom_a76xx(array_at_command);
  HAL_Delay(300);
  snprintf(array_at_command, sizeof(array_at_command),
           "AT+SMCONF=\"WILLRETAIN\",%d\r\n", MQTT_STATUS_RETAIN);
  send_to_simcom_a76xx(array_at_command);
  HAL_Delay(300);
  return true;
#else
  snprintf(array_at_command, sizeof(array_at_command),
           "AT+CMQTTWILLTOPIC=0,%d\r\n", (int)strlen(MQTT_TOPIC_STATUS));
  simcom_clear_rx();
  send_to_simcom_a76xx(array_at_command);
  HAL_Delay(500);
  snprintf(array_at_command, sizeof(array_at_command), "%s\r\n",
           MQTT_TOPIC_STATUS);
  send_to_simcom_a76xx(array_at_command);
  simcom_wait_for_ok(2000);
  if (!simcom_response_ok()) {
    printf("-----------------Set MQTT will topic fail------------------\n");
    simcom_log_response();
    return false;
  }

  snprintf(array_at_command, sizeof(array_at_command),
           "AT+CMQTTWILLMSG=0,%d,%d\r\n", (int)strlen(array_json),
           MQTT_QOS);
  simcom_clear_rx();
  send_to_simcom_a76xx(array_at_command);
  HAL_Delay(500);
  send_to_simcom_a76xx(array_json);
  simcom_wait_for_ok(2000);
  if (!simcom_response_ok()) {
    printf("-----------------Set MQTT will payload fail------------------\n");
    simcom_log_response();
    return false;
  }
  return true;
#endif
}

bool mobi_mqtt_start(void) {
#if SIMCOM_MODEL == a7080
  send_to_simcom_a76xx("AT+CNACT=0,1\r\n");
  HAL_Delay(400);
  send_to_simcom_a76xx("AT+CNACT?\r\n");
  HAL_Delay(400);

  snprintf(array_at_command, sizeof(array_at_command),
           "AT+SMCONF=\"URL\",\"%s\",%d\r\n", MQTT_BROKER_HOST, MQTT_PORT);
  send_to_simcom_a76xx(array_at_command);
  HAL_Delay(400);
  snprintf(array_at_command, sizeof(array_at_command),
           "AT+SMCONF=\"KEEPTIME\",%d\r\n", MQTT_KEEPALIVE_SEC);
  send_to_simcom_a76xx(array_at_command);
  HAL_Delay(400);
  snprintf(array_at_command, sizeof(array_at_command),
           "AT+SMCONF=\"CLIENTID\",\"%s\"\r\n", MQTT_CLIENT_ID);
  send_to_simcom_a76xx(array_at_command);
  HAL_Delay(400);
  snprintf(array_at_command, sizeof(array_at_command),
           "AT+SMCONF=\"USERNAME\",\"%s\"\r\n", MQTT_USER);
  send_to_simcom_a76xx(array_at_command);
  HAL_Delay(400);
  snprintf(array_at_command, sizeof(array_at_command),
           "AT+SMCONF=\"PASSWORD\",\"%s\"\r\n", MQTT_PASS);
  send_to_simcom_a76xx(array_at_command);
  HAL_Delay(400);
  snprintf(array_at_command, sizeof(array_at_command),
           "AT+SMCONF=\"CLEANSS\",%d\r\n", MQTT_CLEAN_SESSION);
  send_to_simcom_a76xx(array_at_command);
  HAL_Delay(400);

  mqtt_configure_last_will();

  simcom_clear_rx();
  send_to_simcom_a76xx("AT+SMCONN\r\n");
  simcom_wait_for_ok(10000);
  return simcom_response_ok();
#else
  simcom_clear_rx();
  send_to_simcom_a76xx("AT+CMQTTSTART\r\n");
  simcom_wait_for_ok_or("+CMQTTSTART: 0", 10000);
  if (!(simcom_response_has("+CMQTTSTART: 0") || simcom_response_ok())) {
    printf("-----------------Start MQTT service fail------------------\n");
    simcom_log_response();
  }
  return simcom_response_has("+CMQTTSTART: 0") || simcom_response_ok();
#endif
}

bool mobi_mqtt_acquire_client(void) {
#if SIMCOM_MODEL == a7080
  return true;
#else
  simcom_clear_rx();
  snprintf(array_at_command, sizeof(array_at_command),
           "+CMQTTACCQ: 0,\"%s\",0\r\n", MQTT_CLIENT_ID);
  send_to_simcom_a76xx("AT+CMQTTACCQ?\r\n");
  HAL_Delay(800);
  if (simcom_response_has(array_at_command)) {
    return true;
  }

  snprintf(array_at_command, sizeof(array_at_command),
           "AT+CMQTTACCQ=0,\"%s\",0\r\n", MQTT_CLIENT_ID);
  simcom_clear_rx();
  send_to_simcom_a76xx(array_at_command);
  simcom_wait_for_ok(3000);
  if (!simcom_response_ok()) {
    printf("-----------------Acquire MQTT client fail------------------\n");
    simcom_log_response();
  }
  return simcom_response_ok();
#endif
}

bool mobi_mqtt_connect(void) {
#if SIMCOM_MODEL == a7080
  return true;
#else
  if (!mqtt_configure_last_will()) {
    printf("-----------------Skip MQTT Last Will setup------------------\n");
  }

  snprintf(array_at_command, sizeof(array_at_command),
           "AT+CMQTTCONNECT=0,\"%s:%d\",%d,%d,\"%s\",\"%s\"\r\n",
           MQTT_HOST, MQTT_PORT, MQTT_KEEPALIVE_SEC, MQTT_CLEAN_SESSION,
           MQTT_USER, MQTT_PASS);
  simcom_clear_rx();
  send_to_simcom_a76xx(array_at_command);
  simcom_wait_for_ok_or("+CMQTTCONNECT: 0,0", 10000);
  if (!(simcom_response_has("+CMQTTCONNECT: 0,0") || simcom_response_ok())) {
    printf("-----------------Connect MQTT server fail------------------\n");
    simcom_log_response();
  }
  return simcom_response_has("+CMQTTCONNECT: 0,0") || simcom_response_ok();
#endif
}

bool mobi_mqtt_subscribe_server_topics(void) {
  if (!mqtt_subscribe_raw(MQTT_TOPIC_CONFIG_SET)) {
    return false;
  }
  if (!mqtt_subscribe_raw(MQTT_TOPIC_CONFIG_GET)) {
    return false;
  }
  return mqtt_subscribe_raw(MQTT_TOPIC_COMMAND_REQUEST);
}

bool mobi_mqtt_after_subscribe_online(void) {
  bool ok = true;
  ok &= mobi_mqtt_publish_status_online();
  ok &= mobi_mqtt_publish_config_state();
  ok &= mobi_mqtt_publish_telemetry();
  return ok;
}

bool mobi_mqtt_publish_status_online(void) {
  build_status_payload("online");
  return mqtt_publish_raw(MQTT_TOPIC_STATUS, array_json, MQTT_QOS,
                          MQTT_STATUS_RETAIN);
}

bool mobi_mqtt_publish_status_offline(void) {
  build_status_payload("offline");
  return mqtt_publish_raw(MQTT_TOPIC_STATUS, array_json, MQTT_QOS,
                          MQTT_STATUS_RETAIN);
}

bool mobi_mqtt_publish_config_state(void) {
  build_config_state_payload();
  return mqtt_publish_raw(MQTT_TOPIC_CONFIG_STATE, array_json, MQTT_QOS,
                          MQTT_CONFIG_STATE_RETAIN);
}

bool mobi_mqtt_publish_telemetry(void) {
  mobi_mqtt_build_telemetry_json(true);
  return mqtt_publish_raw(MQTT_TOPIC_TELEMETRY, array_json, MQTT_QOS,
                          MQTT_RETAIN);
}

bool mobi_mqtt_publish_command_response(const char *request_id,
                                        const char *command,
                                        const char *result,
                                        const char *error_code) {
  cJSON *json = cJSON_CreateObject();
  cJSON_AddStringToObject(json, "request_id", request_id);
  cJSON_AddStringToObject(json, "command", command);
  cJSON_AddStringToObject(json, "result", result);
  if (error_code != NULL) {
    cJSON_AddStringToObject(json, "error_code", error_code);
  }
  json_to_array(json);
  cJSON_Delete(json);
  return mqtt_publish_raw(MQTT_TOPIC_COMMAND_RESPONSE, array_json, MQTT_QOS,
                          MQTT_RETAIN);
}

bool mobi_mqtt_publish_config_response(const char *request_id,
                                       const char *result,
                                       const char *error_code,
                                       const char *message) {
  cJSON *json = cJSON_CreateObject();
  cJSON_AddStringToObject(json, "request_id", request_id);
  cJSON_AddStringToObject(json, "result", result);
  if (error_code != NULL) {
    cJSON_AddStringToObject(json, "error_code", error_code);
  }
  if (message != NULL) {
    cJSON_AddStringToObject(json, "message", message);
  }
  json_to_array(json);
  cJSON_Delete(json);
  return mqtt_publish_raw(MQTT_TOPIC_CONFIG_RESPONSE, array_json, MQTT_QOS,
                          MQTT_RETAIN);
}

bool mobi_mqtt_disconnect(void) {
  mobi_mqtt_publish_status_offline();

#if SIMCOM_MODEL == a7080
  simcom_clear_rx();
  send_to_simcom_a76xx("AT+SMDISC\r\n");
  simcom_wait_for_ok(2000);
  return simcom_response_ok();
#else
  simcom_clear_rx();
  send_to_simcom_a76xx("AT+CMQTTDISC=0,120\r\n");
  simcom_wait_for("+CMQTTDISC: 0,0", 3000);
  if (!simcom_response_has("+CMQTTDISC: 0,0")) {
    return false;
  }

  simcom_clear_rx();
  send_to_simcom_a76xx("AT+CMQTTREL=0\r\n");
  simcom_wait_for_ok(2000);
  if (!simcom_response_ok()) {
    return false;
  }

  simcom_clear_rx();
  send_to_simcom_a76xx("AT+CMQTTSTOP\r\n");
  simcom_wait_for_ok(2000);
  return simcom_response_ok();
#endif
}
