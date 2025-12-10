#ifndef JOYSTICK_STATE_H
#define JOYSTICK_STATE_H

#include "motor_types.h"
#include <stdbool.h>

void joystick_state_init(void);

bool joystick_to_direction(int16_t x_pos, int16_t y_pos, bool btn_a_pressed, Motor_direction *dir, uint8_t *speed);

#endif