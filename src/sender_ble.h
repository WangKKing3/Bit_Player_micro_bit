#ifndef SENDER_BLE_H
#define SENDER_BLE_H

#include "motor_types.h"
#include <stdbool.h>
#include <stdint.h>

void ble_init(void);
void ble_connect(void);
void ble_send_direction(Motor_direction dir, uint8_t speed);
bool ble_is_ready(void);

#endif