#pragma once

#include "main.h"
#include <stdint.h>
//#include "ph_ctrl_types.h"   // ph_ctrl_cfg_t

#ifdef __cplusplus
extern "C" {
#endif



typedef struct {
    ph_zone_t zone;
    uint8_t   phase_on;        // 1=ON, 0=OFF
    uint32_t  phase_start_ms;  // tick bắt đầu phase
} ph_dual_sm_t;


void ph_dual_sm_reset(ph_dual_sm_t *sm);

/**
 * @brief Điều khiển 2 bơm acid/bazo theo ngưỡng + lịch time_on/time_off
 *
 * Bơm bazo  (ph_plus)  : TIM2 CH4
 * Bơm acid  (ph_minus) : TIM2 CH3
 *
 * Duty chung: cfg->pump_power (0..100%)
 * Lịch chung: cfg->time_on / cfg->time_off (giây)
 *
 * @note gọi định kỳ (50~200ms)
 */
void ph_dual_pump_control_step(ph_dual_sm_t *sm,
                               const ph_ctrl_cfg_t *cfg,
                               float ph_meas,
                               TIM_HandleTypeDef *htim2,
                               volatile uint8_t *motor_ph_plus,
                               volatile uint8_t *motor_ph_minus);

#ifdef __cplusplus
}
#endif
