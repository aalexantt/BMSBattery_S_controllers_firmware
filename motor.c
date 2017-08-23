/*
 * EGG OpenSource EBike firmware
 *
 * Copyright (C) Casainho, 2015, 2106, 2017.
 *
 * Released under the GPL License, Version 3
 */

#include <stdint.h>
#include <stdio.h>
#include "stm8s_gpio.h"
#include "stm8s_tim1.h"
#include "motor.h"
#include "gpio.h"
#include "motor.h"
#include "pwm.h"

uint16_t ui16_PWM_cycles_counter = 0;
uint16_t ui16_motor_speed_erps = 0;
uint16_t ui16_speed_inverse = 0;
uint8_t ui8_motor_rotor_position = 0; // in 360/256 degrees
uint8_t ui8_motor_rotor_absolute_position = 0; // in 360/256 degrees
uint8_t ui8_position_correction_value = 0; // in 360/256 degrees
uint16_t ui16_PWM_cycles_counter_total = 0;
uint16_t ui16_PWM_cycles_counter_total_div_4 = 0;
uint8_t ui8_interpolation_angle = 0;

uint16_t ui16_adc_current_phase_B = 0;
uint16_t ui16_adc_current_phase_B_accumulated = 0;
uint16_t ui16_adc_current_phase_B_filtered = 0;

uint8_t motor_state = MOTOR_STATE_COAST;
//uint8_t motor_state = MOTOR_STATE_RUNNING;

int8_t hall_sensors;
int8_t hall_sensors_last = 0;

void TIM1_UPD_OVF_TRG_BRK_IRQHandler(void) __interrupt(TIM1_UPD_OVF_TRG_BRK_IRQHANDLER)
{
  hall_sensors_read_and_action ();

  /****************************************************************
  * Motor control: angle interpolation and PWM control
  */
  motor_fast_loop ();
  /****************************************************************/

  // clear the interrupt pending bit for TIM1
  TIM1_ClearITPendingBit(TIM1_IT_UPDATE);
}

void hall_sensor_init (void)
{
  GPIO_Init(HALL_SENSORS__PORT,
	    (GPIO_Pin_TypeDef)(HALL_SENSOR_A__PIN | HALL_SENSOR_B__PIN | HALL_SENSOR_C__PIN),
	    GPIO_MODE_IN_FL_NO_IT);
}

void hall_sensors_read_and_action (void)
{
  // read hall sensors signal pins and mask other pins
  hall_sensors = (GPIO_ReadInputData (HALL_SENSORS__PORT) & (HALL_SENSORS_MASK));
  if ((hall_sensors != hall_sensors_last) ||
      (motor_state == MOTOR_STATE_COAST)) // let's run the code when motor is stopped/coast so it can pick right motor position for correct startup
  {
    if (motor_state == MOTOR_STATE_COAST)
    {
      motor_state = MOTOR_STATE_STARTUP;
    }

    hall_sensors_last = hall_sensors;

//    if (motor_state != MOTOR_STATE_RUNNING) // needed to reset ui8_position_correction_value
//    {
//      ui8_position_correction_value = 127;
//    }

    switch (hall_sensors)
    {
      case 3:
      if (motor_state != MOTOR_STATE_RUNNING)
      {
	ui8_motor_rotor_absolute_position = (uint8_t) (ANGLE_120 + MOTOR_ROTOR_DELTA_PHASE_ANGLE_RIGHT);
      }
//      debug_pin_set();
      break;

      case 1:
      if (motor_state != MOTOR_STATE_RUNNING)
      {
	ui8_motor_rotor_absolute_position = (uint8_t) (ANGLE_180 + MOTOR_ROTOR_DELTA_PHASE_ANGLE_RIGHT);
      }
      break;

      case 5:
      if (motor_state != MOTOR_STATE_RUNNING)
      {
	ui8_motor_rotor_absolute_position = (uint8_t) (ANGLE_240 + MOTOR_ROTOR_DELTA_PHASE_ANGLE_RIGHT);
      }
      debug_pin_reset();
      break;

      // start of phase B current sinusoid
      case 4:
      ui16_PWM_cycles_counter_total = ui16_PWM_cycles_counter;
      ui16_PWM_cycles_counter_total_div_4 = ui16_PWM_cycles_counter_total >> 2;
      ui16_PWM_cycles_counter = 0;
      ui16_motor_speed_erps = PWM_CYCLES_SECOND / ui16_PWM_cycles_counter_total; // this division takes ~4.2us

      // update to MOTOR_STATE_RUNNING based on motor speed
      if (ui16_PWM_cycles_counter_total < SPEED_INVERSE_INTERPOLATION)
      {
	debug_pin_set();
	motor_state = MOTOR_STATE_RUNNING;
      }

//debug_pin_set();
//	// at this time, phase B current sinusoid should be crossing zero
//	// see if is positive or negative: adjust the ui8_position_correction_value according to
//	//
//	if ((motor_state == MOTOR_STATE_RUNNING) &&
//	    (ui8_adc_read_throttle_busy == 0)) // can't read ADC if is busy
//	{
//	  // find the adc_current_phase_B zero cross
//	  ui16_temp = ADC1_GetConversionValue ();
//	  if (ui16_temp > (512 + 6)) // 512 is the middle value --> zero crossing
//	  {
//	    ui8_position_correction_value--;
//	  }
//	  else if (ui16_temp < (512 - 6))
//	  {
//	    ui8_position_correction_value++;
//	  }
//	}
//debug_pin_reset();

      ui8_motor_rotor_absolute_position = (uint8_t) (ANGLE_300 + MOTOR_ROTOR_DELTA_PHASE_ANGLE_RIGHT);
//      debug_pin_reset();
      break;

      case 6:
      if (motor_state != MOTOR_STATE_RUNNING)
      {
	ui8_motor_rotor_absolute_position = (uint8_t) (ANGLE_1 + MOTOR_ROTOR_DELTA_PHASE_ANGLE_RIGHT);
      }
      break;

      case 2:
      if (motor_state != MOTOR_STATE_RUNNING)
      {
	ui8_motor_rotor_absolute_position = (uint8_t) (ANGLE_60 + MOTOR_ROTOR_DELTA_PHASE_ANGLE_RIGHT);
      }
      break;

      default:
      return;
      break;
    }
  }
}

// runs every 64us (PWM frequency)
void motor_fast_loop (void)
{
//  uint16_t ui16_temp;

  // count number of fast loops / PWM cycles
  if (ui16_PWM_cycles_counter < PWM_CYCLES_COUNTER_MAX)
  {
    ui16_PWM_cycles_counter++;
  }
  else
  {
//debug_pin_set();
    ui16_PWM_cycles_counter = 0;
    ui16_PWM_cycles_counter_total = 0xffff; //(SVM_TABLE_LEN_x1024) / PWM_CYCLES_COUNTER_MAX;
    ui16_speed_inverse = 0xffff;

    // next code is need for motor startup correctly
    ui8_interpolation_angle = 0;
    motor_state = MOTOR_STATE_COAST;
    hall_sensors_read_and_action ();
//debug_pin_reset();
  }

#define DO_INTERPOLATION 1 // may be usefull when debugging
#if DO_INTERPOLATION == 1

  if (motor_state == MOTOR_STATE_RUNNING)
  {
//    if (ui16_PWM_cycles_counter == (ui16_PWM_cycles_counter_total - 1)) // can't read ADC if is busy
//    if (ui16_PWM_cycles_counter == 1) // can't read ADC if is busy
//    {
      // at this time, phase B current sinusoid should be crossing zero
      // see if is positive or negative: adjust the ui8_position_correction_value according to
//    debug_pin_set();
//      // find the adc_current_phase_B zero cross
//      ui16_temp = ADC1_GetConversionValue ();
//      if (ui16_temp > (512)) // 512 is the middle value --> zero crossing
//      {
////	ui8_position_correction_value--;
//	ui8_position_correction_value -= 6;
//      }
//      else if (ui16_temp < (512))
//      {
////	ui8_position_correction_value++;
//	ui8_position_correction_value += 6;
//      }
//    debug_pin_reset();
//    }

    // calculate the interpolation angle
    // interpolation seems a problem when motor starts, so don't do it at very low speed
    ui8_interpolation_angle = (uint8_t) ((((uint32_t) ui16_PWM_cycles_counter) << 8) / ui16_PWM_cycles_counter_total);
    ui8_motor_rotor_position = (uint8_t) (ui8_motor_rotor_absolute_position + ui8_position_correction_value + ui8_interpolation_angle);

//    // Read phase B current only at max value of sinusoid
//    if (ui16_PWM_cycles_counter == ui16_PWM_cycles_counter_total_div_4)
//    {
//      // Read phase B current
//      if (ui8_adc_read_throttle_busy == 0)
//      {
//	ui16_adc_current_phase_B_accumulated = ui16_adc_current_phase_B_accumulated - (ui16_adc_current_phase_B_accumulated >> 4);
//	ui16_adc_current_phase_B_accumulated = ui16_adc_current_phase_B_accumulated + ADC1_GetConversionValue ();
//	ui16_adc_current_phase_B_filtered = ui16_adc_current_phase_B_accumulated >> 4;
//      }
//    }
//ui16_log1 = ui16_motor_speed_erps;
//ui16_log2 = ui8_position_correction_value;
  }
  else
#endif
  {
    ui8_motor_rotor_position = ui8_motor_rotor_absolute_position;

    // reset phase B current value as at very low speeds it has no meaning
    ui16_adc_current_phase_B_accumulated = 0;
    ui16_adc_current_phase_B_filtered = 0;
  }

  pwm_duty_cycle_controller ();
}

int8_t get_motor_rotation_direction (void)
{
  return LEFT;
}
