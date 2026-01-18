#pragma once
#include "main.h"
#include <stdint.h>

typedef struct {
    ph_zone_t zone;
    uint8_t   phase_on;      // 1=ON, 0=OFF
    uint32_t  remain_s;      // còn lại bao nhiêu giây của phase hiện tại
} ph_pump_isr_sm_t;

void ph_pump_isr_init(ph_pump_isr_sm_t *sm);
void ph_pump_isr_request_reset(void);

/**
 * @brief Gọi trong TIM6 mỗi 1 giây.
 * Offline: chạy lịch ON/OFF cho đúng bơm theo pH.
 * Online : tắt bơm + reset lịch.
 */
void ph_pump_isr_tick_1s(ph_pump_isr_sm_t *sm,
                         const ph_ctrl_cfg_t *cfg,
                         float ph_meas,
                         TIM_HandleTypeDef *htim2,
                         volatile uint8_t *motor_ph_plus,
                         volatile uint8_t *motor_ph_minus);
