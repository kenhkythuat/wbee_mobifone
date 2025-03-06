/*
 * level_battery.c
 *
 *  Created on: Aug 23, 2024
 *      Author: admin
 */

#include "main.h"
#include "stdio.h"
extern ADC_HandleTypeDef hadc1;
uint16_t adc_value;
float level_pin;
float percentage_pin;
float array_level_pin[10];
int count = 0;
float array;
float percentage_battery;
float data_percentage_pin;

float map(float in, int x_inmin, int x_inmax, int x_outmin, int x_outmax) {
  return ((in - x_inmin) * (x_outmax - x_outmin) / (x_inmax - x_inmin) + x_outmin);
}

float read_level_pin(void) {

  adc_value = adc_pin_valve;
  // vol 0 -> 3.05 <=> 0 -> 3250 ADC, 3.05v is the actual measurement result
  // on the voltage divider bridge
  level_pin = map(adc_value, 0, 3250, 0, 3.05);
  // 2.5vol -> 3vol => 0% -> 100%
  percentage_pin = ((level_pin - 2.5) * 100) / 0.5;
  if (percentage_pin > 100) {
    percentage_pin = 100;
  } else if (percentage_pin < 0) {
    percentage_pin = 0;
  }
  percentage_battery = percentage_pin;
  // printf("percentage_battery is: %.2f \n", percentage_battery);
  return percentage_battery;
}
