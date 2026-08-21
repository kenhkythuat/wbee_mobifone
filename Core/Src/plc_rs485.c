/*
 * plc_rs485.c
 *
 * Parses passive Modbus ASCII frames sent by the PLC over RS485.
 */
#include "plc_rs485.h"

#include "config.h"
#include "stdio.h"
#include "string.h"

#if SENSOR_DATA_SOURCE == SENSOR_SOURCE_PLC_RS485

static plc_rs485_data_t g_plc_data;
static uint16_t g_plc_timeout_counter_s;
static bool g_plc_timeout_logged;

static int hex_to_nibble(uint8_t c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  return -1;
}

static bool hex_pair_to_byte(uint8_t high, uint8_t low, uint8_t *out) {
  int h = hex_to_nibble(high);
  int l = hex_to_nibble(low);
  if (h < 0 || l < 0) {
    return false;
  }
  *out = (uint8_t)((h << 4) | l);
  return true;
}

static bool modbus_ascii_decode(const uint8_t *data, uint16_t len,
                                uint8_t *out, uint16_t *out_len) {
  uint16_t start = 0;
  uint16_t end = len;
  uint16_t count = 0;

  while (start < len && data[start] != ':') {
    start++;
  }
  if (start >= len) {
    return false;
  }

  start++;
  while (end > start && (data[end - 1] == '\r' || data[end - 1] == '\n' ||
                         data[end - 1] == '\0')) {
    end--;
  }
  if (((end - start) % 2U) != 0U) {
    return false;
  }

  for (uint16_t i = start; i + 1U < end; i += 2U) {
    if (!hex_pair_to_byte(data[i], data[i + 1U], &out[count])) {
      return false;
    }
    count++;
  }

  *out_len = count;
  return count >= 3U;
}

static bool modbus_ascii_lrc_ok(const uint8_t *frame, uint16_t len) {
  uint8_t sum = 0;
  if (len < 3U) {
    return false;
  }

  for (uint16_t i = 0; i < len; i++) {
    sum = (uint8_t)(sum + frame[i]);
  }
  return sum == 0U;
}

static bool get_register_value(const uint16_t *regs, uint16_t start_addr,
                               uint16_t count, uint16_t address,
                               uint16_t *value) {
  if (address == PLC_REG_UNUSED) {
    return false;
  }
  if (address < start_addr || address >= (uint16_t)(start_addr + count)) {
    return false;
  }
  *value = regs[address - start_addr];
  return true;
}

static void update_scaled_value(const uint16_t *regs, uint16_t count,
                                uint16_t start_addr, uint16_t address,
                                float scale, float *value, bool *has_value) {
  uint16_t raw;
  if (get_register_value(regs, start_addr, count, address, &raw) &&
      scale > 0.0f) {
    *value = ((float)raw) / scale;
    *has_value = true;
  }
}

static void update_bit_value(const uint16_t *regs, uint16_t count,
                             uint16_t start_addr, uint16_t address,
                             uint8_t bit_index, uint8_t *value,
                             bool *has_value) {
  uint16_t raw;
  if (get_register_value(regs, start_addr, count, address, &raw) &&
      bit_index < 16U) {
    *value = (uint8_t)((raw >> bit_index) & 0x01U);
    *has_value = true;
  }
}

static bool parse_write_multiple_registers(const uint8_t *frame,
                                           uint16_t frame_len) {
  uint16_t start_addr;
  uint16_t quantity;
  uint8_t byte_count;
  uint16_t regs[PLC_RS485_MAX_REGISTERS];

  if (frame_len < 10U) {
    return false;
  }
  if (frame[0] != PLC_RS485_MODBUS_ADDRESS ||
      frame[1] != PLC_RS485_FUNC_WRITE_MULTIPLE_REGS) {
    return false;
  }

  start_addr = (uint16_t)((frame[2] << 8) | frame[3]);
  quantity = (uint16_t)((frame[4] << 8) | frame[5]);
  byte_count = frame[6];
  if (quantity > PLC_RS485_MAX_REGISTERS || byte_count != (quantity * 2U)) {
    return false;
  }
  if ((uint16_t)(7U + byte_count + 1U) > frame_len) {
    return false;
  }

  for (uint16_t i = 0; i < quantity; i++) {
    uint16_t offset = (uint16_t)(7U + (i * 2U));
    regs[i] = (uint16_t)((frame[offset] << 8) | frame[offset + 1U]);
  }

  printf("PLC RS485 start=%u qty=%u", start_addr, quantity);
  for (uint16_t i = 0; i < quantity; i++) {
    printf(" r%u=%u", (unsigned int)(start_addr + i), regs[i]);
  }
  printf("\r\n");

  update_scaled_value(regs, quantity, start_addr, PLC_REG_PH1, PLC_SCALE_PH1,
                      &g_plc_data.ph1, &g_plc_data.has_ph1);
  update_scaled_value(regs, quantity, start_addr, PLC_REG_DO, PLC_SCALE_DO,
                      &g_plc_data.dissolved_oxygen,
                      &g_plc_data.has_dissolved_oxygen);
  update_scaled_value(regs, quantity, start_addr, PLC_REG_PH2, PLC_SCALE_PH2,
                      &g_plc_data.ph2, &g_plc_data.has_ph2);
  update_scaled_value(regs, quantity, start_addr, PLC_REG_TURBIDITY,
                      PLC_SCALE_TURBIDITY, &g_plc_data.turbidity,
                      &g_plc_data.has_turbidity);
  update_scaled_value(regs, quantity, start_addr, PLC_REG_OZONE,
                      PLC_SCALE_OZONE, &g_plc_data.ozone,
                      &g_plc_data.has_ozone);
  update_scaled_value(regs, quantity, start_addr, PLC_REG_PRESSURE,
                      PLC_SCALE_PRESSURE, &g_plc_data.pressure,
                      &g_plc_data.has_pressure);
  update_bit_value(regs, quantity, start_addr, PLC_REG_X2_STATUS,
                   PLC_BIT_X2_FULL_TANK, &g_plc_data.x2_full_tank,
                   &g_plc_data.has_x2_full_tank);
  update_bit_value(regs, quantity, start_addr, PLC_REG_Y1_STATUS,
                   PLC_BIT_Y1_MODULE_ON, &g_plc_data.y1_module_on,
                   &g_plc_data.has_y1_module_on);
  update_bit_value(regs, quantity, start_addr, PLC_REG_Y24_STATUS,
                   PLC_BIT_Y24_MODULE_ON, &g_plc_data.y24_module_on,
                   &g_plc_data.has_y24_module_on);

  g_plc_data.is_online = true;
  g_plc_timeout_counter_s = 0;
  g_plc_timeout_logged = false;
  printf("PLC RS485 data updated\r\n");
  return true;
}

void plc_rs485_init(void) {
  memset(&g_plc_data, 0, sizeof(g_plc_data));
  g_plc_timeout_counter_s = 0;
  g_plc_timeout_logged = false;
}

void plc_rs485_on_uart_rx(const uint8_t *data, uint16_t len) {
  uint8_t frame[64];
  uint16_t frame_len = 0;
  uint16_t pos = 0;
  bool parsed_any = false;

  while (pos < len) {
    uint16_t start = pos;
    uint16_t end;

    while (start < len && data[start] != ':') {
      start++;
    }
    if (start >= len) {
      break;
    }

    end = (uint16_t)(start + 1U);
    while (end < len && data[end] != ':') {
      end++;
    }

    if (!modbus_ascii_decode(&data[start], (uint16_t)(end - start), frame,
                             &frame_len)) {
      printf("PLC RS485 frame decode fail\r\n");
      pos = end;
      continue;
    }
    if (!modbus_ascii_lrc_ok(frame, frame_len)) {
      printf("PLC RS485 LRC error\r\n");
      pos = end;
      continue;
    }
    if (!parse_write_multiple_registers(frame, frame_len)) {
      printf("PLC RS485 unsupported frame\r\n");
      pos = end;
      continue;
    }

    parsed_any = true;
    pos = end;
  }

  if (!parsed_any) {
    printf("PLC RS485 no valid frame\r\n");
  }
}

void plc_rs485_tick_1s(void) {
  if (g_plc_timeout_counter_s < PLC_RS485_TIMEOUT_SEC) {
    g_plc_timeout_counter_s++;
  }
  if (g_plc_timeout_counter_s >= PLC_RS485_TIMEOUT_SEC &&
      !g_plc_timeout_logged) {
    g_plc_data.is_online = false;
    g_plc_data.has_ph1 = false;
    g_plc_data.has_dissolved_oxygen = false;
    g_plc_data.has_ph2 = false;
    g_plc_data.has_turbidity = false;
    g_plc_data.has_ozone = false;
    g_plc_data.has_pressure = false;
    g_plc_data.has_x2_full_tank = false;
    g_plc_data.has_y1_module_on = false;
    g_plc_data.has_y24_module_on = false;
    g_plc_timeout_logged = true;
    printf("PLC RS485 timeout: no data for %d seconds\r\n",
           PLC_RS485_TIMEOUT_SEC);
  }
}

bool plc_rs485_get_data(plc_rs485_data_t *out) {
  if (out == NULL) {
    return false;
  }
  *out = g_plc_data;
  return g_plc_data.is_online;
}

bool plc_rs485_is_online(void) {
  return g_plc_data.is_online;
}

#else

void plc_rs485_init(void) {}
void plc_rs485_on_uart_rx(const uint8_t *data, uint16_t len) {
  (void)data;
  (void)len;
}
void plc_rs485_tick_1s(void) {}
bool plc_rs485_get_data(plc_rs485_data_t *out) {
  if (out != NULL) {
    memset(out, 0, sizeof(*out));
  }
  return false;
}
bool plc_rs485_is_online(void) {
  return false;
}

#endif
