#include "joystick_state.h"

#define JOY_CENTER 512
#define JOY_DEADZONE 60
#define JOY_MAX 1023

static Motor_direction current_dir = Stop;

void joystick_state_init(void) {
	current_dir = Stop;
}

bool joystick_to_direction(int16_t x_pos, int16_t y_pos, bool btn_a, Motor_direction *dir, uint8_t *speed) {

	Motor_direction new_dir = Stop;
	int8_t new_speed = 0;

	if (!btn_a){
		new_dir = Idle;
		new_speed = 0;
		goto done;
	}

	// Calculate forward/backward and turning components
	int32_t fwd = 0;
	if (y_pos > JOY_CENTER + JOY_DEADZONE) {
		fwd = ((y_pos - JOY_CENTER) * 100) / (JOY_MAX - JOY_CENTER);
	} else if (y_pos < JOY_CENTER - JOY_DEADZONE) {
		fwd = -((JOY_CENTER - y_pos) * 100) / (JOY_CENTER);
	}

	// Calculate turning component
	int32_t turn = 0;
	if (x_pos > JOY_CENTER + JOY_DEADZONE) {
		turn = ((x_pos - JOY_CENTER) * 100) / (JOY_MAX - JOY_CENTER);
	} else if (x_pos < JOY_CENTER - JOY_DEADZONE) {
		turn = -((JOY_CENTER - x_pos) * 100) / (JOY_CENTER);
	}
	
	if (fwd == 0 && turn == 0) {
		new_dir = Stop;
		new_speed = 0;
		goto done;
	}

    int32_t abs_fwd = (fwd >= 0) ? fwd : -fwd;
    int32_t abs_turn = (turn >= 0) ? turn : -turn;

	new_speed = (abs_fwd > abs_turn) ? abs_fwd : abs_turn;
	if (new_speed > 100) new_speed = 100;

	if (abs_fwd < 25 && abs_turn > 10) {
		new_dir = (turn > 0) ? Rotate_Right : Rotate_Left;
		goto done;
	} 

	if (fwd > 0) {
		if (abs_turn > 50)
			new_dir = (turn > 0) ? Right : Left;
		else {
			new_dir = Forward;
		}
	} else if (fwd < 0) {
		if (abs_turn > 50)
			new_dir = (turn > 0) ? Right : Left;
		else {
			new_dir = Backward;
		}
	} else {
		if (abs_turn > 10) {
            new_dir = (turn > 0) ? Rotate_Right : Rotate_Left;
        }
	}

done:
 *dir = new_dir;
 *speed = new_speed;

 bool changed = (new_dir != current_dir);
 current_dir = new_dir;
 return changed;
}