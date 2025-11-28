
#ifndef JOYSTICK_BLE_H
#define JOYSTICK_BLE_H

#include <stdint.h>
#include <stdbool.h>

/* Initialiser Bluetooth */
void ble_init(void);

/* Send joystick data via Bluetooth */
void ble_send_joystick_data(int16_t x_pos, int16_t y_pos);

/* Start tilkobling (scanning/advertising) */
void ble_connect(void);

/* Avbryt tilkobling */
void ble_cancel_connect(void);

/* Sjekk om Bluetooth er klar til å sende data */
bool ble_is_ready(void);

#endif /* JOYSTICK_BLE_H */
