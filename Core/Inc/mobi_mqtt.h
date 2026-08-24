/*
 * mobi_mqtt.h
 *
 * MQTT flow for Mobi water monitoring protocol v1.0.
 */
#ifndef INC_MOBI_MQTT_H_
#define INC_MOBI_MQTT_H_

#include "stdbool.h"
#include "stdint.h"

typedef enum {
  MOBI_MQTT_RESULT_OK = 0,
  MOBI_MQTT_RESULT_ERROR
} mobi_mqtt_result_t;

bool mobi_mqtt_start(void);
bool mobi_mqtt_acquire_client(void);
bool mobi_mqtt_connect(void);
bool mobi_mqtt_subscribe_server_topics(void);
bool mobi_mqtt_after_subscribe_online(void);
bool mobi_mqtt_publish_telemetry(void);
bool mobi_mqtt_publish_config_state(void);
bool mobi_mqtt_publish_status_online(void);
bool mobi_mqtt_publish_status_offline(void);
bool mobi_mqtt_disconnect(void);
void mobi_mqtt_build_telemetry_json(bool refresh_rssi);

bool mobi_mqtt_publish_command_response(const char *request_id,
                                        const char *command,
                                        const char *result,
                                        const char *error_code);
bool mobi_mqtt_publish_config_response(const char *request_id,
                                       const char *result,
                                       const char *error_code,
                                       const char *message);

#endif /* INC_MOBI_MQTT_H_ */
