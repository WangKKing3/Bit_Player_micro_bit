#ifndef JOYSTICK_BLE_H
#define JOYSTICK_BLE_H

#include <stdbool.h>
#include <stdint.h>

#define BTN_A_MASK (1 << 0)
#define BTN_B_MASK (1 << 1)

void ble_init(void);
void ble_connect(void);
void ble_cancel_connect(void);
void ble_send_joystick_data(int16_t x_pos, int16_t y_pos, uint8_t buttons);
bool ble_is_ready(void);

#endif