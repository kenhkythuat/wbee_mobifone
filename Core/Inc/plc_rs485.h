/*
 * plc_rs485.h
 *
 * Passive PLC RS485 data receiver.
 */
#ifndef INC_PLC_RS485_H_
#define INC_PLC_RS485_H_

#include "stdbool.h"
#include "stdint.h"

typedef struct {
  float ph1;
  float dissolved_oxygen;
  float ph2;
  float turbidity;
  float ozone;
  float pressure;
  float temp_data_1;
  float temp_data_2;
  float temp_data_3;
  uint8_t input_x;
  uint8_t output_1;
  uint8_t output_2;
  uint8_t error_code;
  bool has_ph1;
  bool has_dissolved_oxygen;
  bool has_ph2;
  bool has_turbidity;
  bool has_ozone;
  bool has_pressure;
  bool has_temp_data_1;
  bool has_temp_data_2;
  bool has_temp_data_3;
  bool has_input_x;
  bool has_output_1;
  bool has_output_2;
  bool has_error_code;
  bool is_online;
} plc_rs485_data_t;

void plc_rs485_init(void);
void plc_rs485_on_uart_rx(const uint8_t *data, uint16_t len);
void plc_rs485_tick_1s(void);
bool plc_rs485_get_data(plc_rs485_data_t *out);
bool plc_rs485_is_online(void);

#endif /* INC_PLC_RS485_H_ */
