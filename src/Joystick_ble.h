#ifndef JOYSTICK_BLE_H
#define JOYSTICK_BLE_H

#include <stdint.h>
#include <stdbool.h>

/* Button masks */
#define BTN_A_MASK  (1 << 0)
#define BTN_B_MASK  (1 << 1)

/* Initialiser Bluetooth */
void ble_init(void);

/* Send joystick data via Bluetooth (med knapper) */
void ble_send_joystick_data(int16_t x_pos, int16_t y_pos, uint8_t buttons);

/* Start tilkobling (advertising) */
void ble_connect(void);

/* Avbryt tilkobling */
void ble_cancel_connect(void);

/* Sjekk om Bluetooth er klar til å sende data */
bool ble_is_ready(void);

#endif /* JOYSTICK_BLE_H */