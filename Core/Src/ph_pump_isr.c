#include "ph_pump_isr.h"

static volatile uint8_t s_reset_req = 0;

void ph_pump_isr_request_reset(void) { s_reset_req = 1; }

void ph_pump_isr_init(ph_pump_isr_sm_t *sm)
{
    sm->zone = PH_ZONE_OK;
    sm->phase_on = 0;
    sm->remain_s = 0;
    s_reset_req = 0;
}

static uint32_t duty_percent_to_ccr(TIM_HandleTypeDef *htim, uint32_t percent)
{
    if(percent > 100) percent = 100;
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(htim);
    return (arr * percent) / 100;
}

static void pumps_all_off(TIM_HandleTypeDef *htim2,
                          volatile uint8_t *motor_ph_plus,
                          volatile uint8_t *motor_ph_minus)
{
    if(*motor_ph_plus)  { HAL_TIM_PWM_Stop(htim2, TIM_CHANNEL_4); *motor_ph_plus = 0; }
    if(*motor_ph_minus) { HAL_TIM_PWM_Stop(htim2, TIM_CHANNEL_3); *motor_ph_minus = 0; }
}

static void pump_plus_on(TIM_HandleTypeDef *htim2, uint32_t duty,
                         volatile uint8_t *motor_ph_plus,
                         volatile uint8_t *motor_ph_minus)
{
    // bazo ON (CH4), acid OFF (CH3)
    if(!*motor_ph_plus) { HAL_TIM_PWM_Start(htim2, TIM_CHANNEL_4); *motor_ph_plus = 1; }
    __HAL_TIM_SET_COMPARE(htim2, TIM_CHANNEL_4, duty);

    if(*motor_ph_minus) { HAL_TIM_PWM_Stop(htim2, TIM_CHANNEL_3); *motor_ph_minus = 0; }
}

static void pump_minus_on(TIM_HandleTypeDef *htim2, uint32_t duty,
                          volatile uint8_t *motor_ph_plus,
                          volatile uint8_t *motor_ph_minus)
{
    // acid ON (CH3), bazo OFF (CH4)
    if(!*motor_ph_minus) { HAL_TIM_PWM_Start(htim2, TIM_CHANNEL_3); *motor_ph_minus = 1; }
    __HAL_TIM_SET_COMPARE(htim2, TIM_CHANNEL_3, duty);

    if(*motor_ph_plus) { HAL_TIM_PWM_Stop(htim2, TIM_CHANNEL_4); *motor_ph_plus = 0; }
}

void ph_pump_isr_tick_1s(ph_pump_isr_sm_t *sm,
                         const ph_ctrl_cfg_t *cfg,
                         float ph_meas,
                         TIM_HandleTypeDef *htim2,
                         volatile uint8_t *motor_ph_plus,
                         volatile uint8_t *motor_ph_minus)
{
    if(!sm || !cfg || !htim2) return;

    if(s_reset_req) { // reset theo yêu cầu từ main (khi UART update cfg/mode)
        s_reset_req = 0;
        sm->zone = PH_ZONE_OK;
        sm->phase_on = 0;
        sm->remain_s = 0;
        pumps_all_off(htim2, motor_ph_plus, motor_ph_minus);
        return;
    }

    // Online -> tắt bơm + reset lịch
    if(cfg->control_mode != 0) {
        sm->zone = PH_ZONE_OK;
        sm->phase_on = 0;
        sm->remain_s = 0;
        pumps_all_off(htim2, motor_ph_plus, motor_ph_minus);
        return;
    }

    // Offline
    float ph_low = cfg->ph_low;
    float ph_high = cfg->ph_high;
    if(ph_low > ph_high) { float t=ph_low; ph_low=ph_high; ph_high=t; }

    ph_zone_t new_zone = PH_ZONE_OK;
    if(ph_meas > ph_high) new_zone = PH_ZONE_HIGH;     // acid theo lịch
    else if(ph_meas < ph_low) new_zone = PH_ZONE_LOW;  // bazo theo lịch

    uint32_t ton = cfg->time_on  ? cfg->time_on  : 1;
    uint32_t tof = cfg->time_off ? cfg->time_off : 1;
    uint32_t duty = duty_percent_to_ccr(htim2, cfg->pump_power);

    // vào vùng OK -> tắt hết + reset
    if(new_zone == PH_ZONE_OK) {
        sm->zone = PH_ZONE_OK;
        sm->phase_on = 0;
        sm->remain_s = 0;
        pumps_all_off(htim2, motor_ph_plus, motor_ph_minus);
        return;
    }

    // đổi vùng (LOW <-> HIGH): bắt đầu ON ngay
    if(new_zone != sm->zone) {
        sm->zone = new_zone;
        sm->phase_on = 1;
        sm->remain_s = ton;

//        if(sm->zone == PH_ZONE_LOW)  pump_plus_on(htim2, duty, motor_ph_plus, motor_ph_minus);
//        else                         pump_minus_on(htim2, duty, motor_ph_plus, motor_ph_minus);
		if(sm->zone == PH_ZONE_LOW)  pump_minus_on(htim2, duty, motor_ph_plus, motor_ph_minus);
		else                         pump_plus_on(htim2, duty, motor_ph_plus, motor_ph_minus);
        return;
    }

    // cùng vùng: chạy countdown 1s
    if(sm->remain_s > 0) sm->remain_s--;

    if(sm->remain_s == 0)
    {
        if(sm->phase_on) {
            // ON -> OFF
            pumps_all_off(htim2, motor_ph_plus, motor_ph_minus);
            sm->phase_on = 0;
            sm->remain_s = tof;
        } else {
            // OFF -> ON
            sm->phase_on = 1;
            sm->remain_s = ton;

//            if(sm->zone == PH_ZONE_LOW)  pump_plus_on(htim2, duty, motor_ph_plus, motor_ph_minus);
//            else                         pump_minus_on(htim2, duty, motor_ph_plus, motor_ph_minus);
                        if(sm->zone == PH_ZONE_LOW)  pump_minus_on(htim2, duty, motor_ph_plus, motor_ph_minus);
                        else                         pump_plus_on(htim2, duty, motor_ph_plus, motor_ph_minus);

        }
    }
}
