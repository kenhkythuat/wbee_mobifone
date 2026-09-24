#pragma once
#include "main.h"
#include <stdint.h>
#include <stdbool.h>
#include "main.h"

extern ph_ctrl_cfg_t g_cfg;

bool cfg_load_from_flash(void);
bool cfg_save_to_flash(void);
void cfg_print(const ph_ctrl_cfg_t *cfg, const char *tag);

const char *device_serial_get(void);
bool device_serial_is_valid(const char *serial_number);
bool device_serial_save(const char *serial_number);
