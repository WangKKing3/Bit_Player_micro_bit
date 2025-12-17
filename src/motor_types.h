/*
* Acknowledgement:
* This file includes example code derrived from https://github.com/OskeLTU
* Driving_car motor_controls.h
*/

#ifndef MOTOR_TYPES_H
#define MOTOR_TYPES_H
#include <stdint.h>
typedef enum {
    Forward,
    Backward,
    Rotate_Right,
    Rotate_Left,
    Right,
    Left,
    Stop,
    Idle
} Motor_direction;

struct drive_packet {
    uint8_t direction;  // Motor_direction
    uint8_t speed;      // Speed value (0-255)
} __packed;
 #endif // MOTOR_TYPES_H