#include "ph_pump_scheduler.h"

static inline uint32_t sec_to_ms(uint32_t s) { return s * 1000UL; }

static uint32_t duty_percent_to_ccr(TIM_HandleTypeDef *htim, uint32_t percent)
{
    if (percent > 100) percent = 100;
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(htim);
    return (arr * percent) / 100;
}

static void pumps_all_off(TIM_HandleTypeDef *htim2,
                          volatile uint8_t *motor_ph_plus,
                          volatile uint8_t *motor_ph_minus)
{
    if (*motor_ph_plus) {
        HAL_TIM_PWM_Stop(htim2, TIM_CHANNEL_4);
        *motor_ph_plus = 0;
    }
    if (*motor_ph_minus) {
        HAL_TIM_PWM_Stop(htim2, TIM_CHANNEL_3);
        *motor_ph_minus = 0;
    }
}

static void pump_plus_set(TIM_HandleTypeDef *htim2, uint32_t duty,
                          volatile uint8_t *motor_ph_plus,
                          volatile uint8_t *motor_ph_minus)
{
    // bazo ON, acid OFF
    if (!*motor_ph_plus) {
        HAL_TIM_PWM_Start(htim2, TIM_CHANNEL_4);
        *motor_ph_plus = 1;
    }
    __HAL_TIM_SET_COMPARE(htim2, TIM_CHANNEL_4, duty);

    if (*motor_ph_minus) {
        HAL_TIM_PWM_Stop(htim2, TIM_CHANNEL_3);
        *motor_ph_minus = 0;
    }


}

static void pump_minus_set(TIM_HandleTypeDef *htim2, uint32_t duty,
                           volatile uint8_t *motor_ph_plus,
                           volatile uint8_t *motor_ph_minus)
{
    // acid ON, bazo OFF
    if (!*motor_ph_minus) {
        HAL_TIM_PWM_Start(htim2, TIM_CHANNEL_3);
        *motor_ph_minus = 1;
    }
    __HAL_TIM_SET_COMPARE(htim2, TIM_CHANNEL_3, duty);

    if (*motor_ph_plus) {
        HAL_TIM_PWM_Stop(htim2, TIM_CHANNEL_4);
        *motor_ph_plus = 0;
    }


}

void ph_dual_sm_reset(ph_dual_sm_t *sm)
{
    if(!sm) return;
    sm->zone = PH_ZONE_OK;
    sm->phase_on = 0;
    sm->phase_start_ms = HAL_GetTick();
}

void ph_dual_pump_control_step(ph_dual_sm_t *sm,
                               const ph_ctrl_cfg_t *cfg,
                               float ph_meas,
                               TIM_HandleTypeDef *htim2,
                               volatile uint8_t *motor_ph_plus,
                               volatile uint8_t *motor_ph_minus)
{
    if(!sm || !cfg || !htim2 || !motor_ph_plus || !motor_ph_minus) return;

    // chống nhập ngược
    float ph_low  = cfg->ph_low;
    float ph_high = cfg->ph_high;
    if(ph_low > ph_high) { float t = ph_low; ph_low = ph_high; ph_high = t; }

    // xác định vùng
    ph_zone_t new_zone = PH_ZONE_OK;
    if (ph_meas > ph_high) new_zone = PH_ZONE_HIGH;
    else if (ph_meas < ph_low) new_zone = PH_ZONE_LOW;
    else new_zone = PH_ZONE_OK;

    uint32_t now = HAL_GetTick();

    // duty chung 0..100%
    uint32_t duty = duty_percent_to_ccr(htim2, cfg->pump_power);

    // time_on/off (giây) -> ms
    uint32_t ton_ms  = sec_to_ms(cfg->time_on);
    uint32_t toff_ms = sec_to_ms(cfg->time_off);
    if (ton_ms == 0)  ton_ms  = 1;
    if (toff_ms == 0) toff_ms = 1;

    // nếu đổi vùng -> reset lịch và bắt đầu ON ngay (nếu LOW/HIGH)
    if (new_zone != sm->zone) {
        sm->zone = new_zone;
        sm->phase_on = (new_zone == PH_ZONE_OK) ? 0 : 1;
        sm->phase_start_ms = now;

        if (new_zone == PH_ZONE_OK) {
            pumps_all_off(htim2, motor_ph_plus, motor_ph_minus);
            return;
        }

        // bắt đầu ON ngay theo vùng
//        if (new_zone == PH_ZONE_LOW)  pump_plus_set(htim2, duty, motor_ph_plus, motor_ph_minus);
//        else                          pump_minus_set(htim2, duty, motor_ph_plus, motor_ph_minus);
                if (new_zone == PH_ZONE_LOW)  pump_minus_set(htim2, duty, motor_ph_plus, motor_ph_minus);
                else                          pump_plus_set(htim2, duty, motor_ph_plus, motor_ph_minus);
        return;
    }

    // vùng OK: tắt hết và giữ state
    if (sm->zone == PH_ZONE_OK) {
        pumps_all_off(htim2, motor_ph_plus, motor_ph_minus);
        sm->phase_on = 0;
        sm->phase_start_ms = now;
        return;
    }

    // vùng LOW/HIGH: chạy theo lịch ON/OFF
    if (sm->phase_on)
    {
        // đảm bảo đang ON đúng bơm
        if (sm->zone == PH_ZONE_LOW)  pump_minus_set(htim2, duty, motor_ph_plus, motor_ph_minus);
        else                          pump_plus_set(htim2, duty, motor_ph_plus, motor_ph_minus);

        if ((uint32_t)(now - sm->phase_start_ms) >= ton_ms) {
            // chuyển OFF: tắt cả 2 (vì trong OFF phase không bơm nào chạy)
            pumps_all_off(htim2, motor_ph_plus, motor_ph_minus);
            sm->phase_on = 0;
            sm->phase_start_ms = now;
        }
    }
    else
    {
        // OFF phase
        if ((uint32_t)(now - sm->phase_start_ms) >= toff_ms) {
            // chuyển ON
            if (sm->zone == PH_ZONE_LOW)  pump_minus_set(htim2, duty, motor_ph_plus, motor_ph_minus);
            else                          pump_plus_set(htim2, duty, motor_ph_plus, motor_ph_minus);

            sm->phase_on = 1;
            sm->phase_start_ms = now;
        } else {
            // đảm bảo OFF phase thì cả 2 đều tắt
            pumps_all_off(htim2, motor_ph_plus, motor_ph_minus);
        }
    }
}
