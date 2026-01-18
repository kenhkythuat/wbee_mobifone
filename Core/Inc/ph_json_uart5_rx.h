#pragma once

#include "main.h"
#include <stdint.h>
#include <stdbool.h>
#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PH_JSON_UART_RX_LINE_MAX
#define PH_JSON_UART_RX_LINE_MAX   256
#endif

int parse_control_json(const char *json, ph_ctrl_cfg_t *out);
/**
 * @brief Init receiver (start UART Receive IT 1 byte)
 */
void ph_json_uart5_rx_init(UART_HandleTypeDef *huart);

/**
 * @brief Call inside HAL_UART_RxCpltCallback(huart)
 *        (This module only handles the UART you passed in init)
 */
void ph_json_uart5_rx_irq(UART_HandleTypeDef *huart);

/**
 * @brief Call in main loop. If a full line JSON received + parsed OK,
 *        this returns true and outputs cfg.
 */
bool ph_json_uart5_rx_poll(ph_ctrl_cfg_t *out);

/**
 * @brief Optional: get last raw line for debug
 */
const char *ph_json_uart5_rx_get_last_line(void);

int parse_mode_json(const char *json, uint32_t *mode_out);

#ifdef __cplusplus
}
#endif
