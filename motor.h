/*
 * EGG OpenSource EBike firmware
 *
 * Copyright (C) Casainho, 2015, 2106, 2017.
 *
 * Released under the GPL License, Version 3
 */

#ifndef _MOTOR_H
#define _MOTOR_H

#include "main.h"
#include "interrupts.h"

// direction of motor movement
#define RIGHT 		1
#define LEFT 		2

// motor states
#define MOTOR_STATE_COAST 0
#define MOTOR_STATE_STARTUP 1
#define MOTOR_STATE_RUNNING 2

extern uint8_t ui8_motor_rotor_position;
extern uint8_t ui8_position_correction_value;
extern uint16_t ui16_speed_inverse;
extern uint16_t ui16_PWM_cycles_counter_total;

void hall_sensor_init (void);
void hall_sensors_read_and_action (void);
void motor_fast_loop (void);
int8_t get_motor_rotation_direction (void);

#endif /* _MOTOR_H_ */
