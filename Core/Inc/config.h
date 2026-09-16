/*
 * config.h
 *
 *  Created on: AUG 1, 2024
 *      Author: thuanphat7
 */
#ifndef INC_CONFIG_H_
#define INC_CONFIG_H_

#include "stdio.h"
#include <string.h>
#include <main.h>

// Codename of the farm, where we deploy this node to.

#define VERSION_WBEE "2.9"

#define OTA_ENABLE 1
#define OTA_MANIFEST_URL "https://raw.githubusercontent.com/kenhkythuat/wbee_mobifone/feature/ota_mobifone/ota/manifest.json"
#define OTA_MANIFEST_URL_FALLBACK_1 "https://raw.githubusercontent.com/kenhkythuat/wbee_mobifone/main/ota/manifest.json"
#define OTA_MANIFEST_URL_FALLBACK_2 "https://raw.githubusercontent.com/kenhkythuat/wbee_mobifone/master/ota/manifest.json"
#define OTA_DEVICE_ID "wbee-stm32f103ret6"

#include "ota_layout.h"

#define a7672s 1  // 4G
#define a7670c 2  // 4G
#define a7670sa 3 // 4G
#define a7080 4  // NB-IOT
#define a7680 5  // 4G

#define SIMCOM_MODEL a7680 // #default is a7670 if you use model other please choose enter your model
#define SAVE_LOAD false
#define INTERVAL_PUPLISH_DATA 15 // the time the device sends data to the server, If the sending time is over 60 seconds, the sensor will go into deep sleep.

// Serial number. Must be lower case.
#ifndef SERIAL_NUMBER
  #define SERIAL_NUMBER "wb000002"
#endif

#define true 1
#define false 0

#define ph_fuvitech false
#define ec_fuvitech false
#define do_fuvitech false

#define ph_rika500_12 false
#define ec_rika500_13 false
#define do_rika500_04 true

#define SENSOR_SOURCE_DIRECT 0
#define SENSOR_SOURCE_PLC_RS485 1
#define SENSOR_DATA_SOURCE SENSOR_SOURCE_PLC_RS485

#define PLC_RS485_TIMEOUT_SEC 300
#define PLC_RS485_MAX_REGISTERS 16
#define PLC_RS485_MODBUS_ADDRESS 2
#define PLC_RS485_FUNC_WRITE_MULTIPLE_REGS 0x10
#define PLC_RS485_BAUDRATE 9600
#define PLC_REG_UNUSED 0xFFFFU

#define PLC_REG_PH1 0
#define PLC_REG_PH2 1
#define PLC_REG_DO 2
#define PLC_REG_OZONE 3
#define PLC_REG_PRESSURE 4
#define PLC_REG_TURBIDITY 0x20U
#define PLC_REG_TEMP_DATA_1 0x21U
#define PLC_REG_TEMP_DATA_2 0x22U
#define PLC_REG_TEMP_DATA_3 0x23U

#define PLC_REG_INPUT_X 5
#define PLC_REG_OUTPUT_1 6
#define PLC_REG_OUTPUT_2 7
#define PLC_REG_ERROR_CODE 9

#define PLC_SCALE_PH1 10.0f
#define PLC_SCALE_PH2 10.0f
#define PLC_SCALE_DO 1.0f
#define PLC_SCALE_OZONE 1.0f
#define PLC_SCALE_PRESSURE 1.0f
#define PLC_SCALE_TURBIDITY 1.0f
#define PLC_SCALE_TEMP_DATA_1 1.0f
#define PLC_SCALE_TEMP_DATA_2 1.0f
#define PLC_SCALE_TEMP_DATA_3 1.0f

#define duty_cycles_ph 50
#define duty_cycles_ec 50
#define duty_cycles_x 50



//#define FARM "demox"
//#define MQTT_USER "node" 		// User - connect to MQTT broker
//#define MQTT_PASS "654321"		// Password - connect to MQTT broker

#define FARM "mobi/water"
#define MQTT_USER "admin"       // User - connect to MQTT broker
#define MQTT_PASS "admin123"		// Password - connect to MQTT broker

#define MQTT_TOPIC_TELEMETRY FARM "/" SERIAL_NUMBER "/telemetry"
#define MQTT_TOPIC_STATUS FARM "/" SERIAL_NUMBER "/status"
#define MQTT_TOPIC_CONFIG_SET FARM "/" SERIAL_NUMBER "/config/set"
#define MQTT_TOPIC_CONFIG_GET FARM "/" SERIAL_NUMBER "/config/get"
#define MQTT_TOPIC_CONFIG_STATE FARM "/" SERIAL_NUMBER "/config/state"
#define MQTT_TOPIC_CONFIG_RESPONSE FARM "/" SERIAL_NUMBER "/config/response"
#define MQTT_TOPIC_COMMAND_REQUEST FARM "/" SERIAL_NUMBER "/command/request"
#define MQTT_TOPIC_COMMAND_RESPONSE FARM "/" SERIAL_NUMBER "/command/response"
/** MQTT
 * Mobi water monitoring broker.
 */
#define MQTT_BROKER_HOST "42.1.65.167"             // MQTT broker host/IP without scheme
#define MQTT_HOST "tcp://" MQTT_BROKER_HOST        // MQTT broker URL for CMQTTCONNECT

#define MQTT_CLIENT_ID  "mobi-" SERIAL_NUMBER
#define MQTT_PORT 1883
#define MQTT_KEEPALIVE_SEC 60
#define MQTT_QOS 0
#define MQTT_RETAIN 0
#define MQTT_CLEAN_SESSION 1
#define MQTT_STATUS_RETAIN 1
#define MQTT_CONFIG_STATE_RETAIN 1
#define TELEMETRY_INTERVAL_DEFAULT_S 15
#define SENSOR_SAMPLE_INTERVAL_DEFAULT_S 10

#define TIME_PERIOD ((2000000*INTERVAL_PUPLISH_DATA)/60000)-1


#endif /* INC_CONFIG_H_ */
