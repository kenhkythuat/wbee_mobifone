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
  uint8_t x2_full_tank;
  uint8_t y1_module_on;
  uint8_t y24_module_on;
  bool has_ph1;
  bool has_dissolved_oxygen;
  bool has_ph2;
  bool has_turbidity;
  bool has_ozone;
  bool has_pressure;
  bool has_x2_full_tank;
  bool has_y1_module_on;
  bool has_y24_module_on;
  bool is_online;
} plc_rs485_data_t;

void plc_rs485_init(void);
void plc_rs485_on_uart_rx(const uint8_t *data, uint16_t len);
void plc_rs485_tick_1s(void);
bool plc_rs485_get_data(plc_rs485_data_t *out);
bool plc_rs485_is_online(void);

#endif /* INC_PLC_RS485_H_ */
