/*
 * Joystick Bluetooth Implementation
 * For micro:bit v2 with Zephyr SDK v2.5.1
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <string.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>

#include "joystick_ble.h"

/* Custom UUID for joystick service */
#define JOYSTICK_SVC_UUID \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)

#define JOYSTICK_CHR_UUID \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1)

static struct bt_uuid_128 joystick_svc_uuid = BT_UUID_INIT_128(JOYSTICK_SVC_UUID);
static struct bt_uuid_128 joystick_chr_uuid = BT_UUID_INIT_128(JOYSTICK_CHR_UUID);

/* Advertising data */
static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, JOYSTICK_SVC_UUID),
};

/* Connection handle */
static struct bt_conn *default_conn;

/* GATT attribute for local service */
static const struct bt_gatt_attr *local_attr;

/* Status flags */
static bool remote_ready;
static bool is_connected;

/* Joystick data structure */
struct joystick_data {
	int16_t x_pos;
	int16_t y_pos;
} __packed;

/* Send joystick data via Bluetooth */
void ble_send_joystick_data(int16_t x_pos, int16_t y_pos)
{
	struct joystick_data data = {
		.x_pos = x_pos,
		.y_pos = y_pos,
	};
	int err;

	if (!default_conn || !remote_ready) {
		/* Ikke klar til å sende ennå */
		return;
	}

	err = bt_gatt_notify(default_conn, local_attr, &data, sizeof(data));
	if (err) {
		printk("GATT notify failed (err %d)\n", err);
	}
}

/* Check if ready to send data */
bool ble_is_ready(void)
{
	return (default_conn != NULL && remote_ready);
}

/* CCC (Client Characteristic Configuration) changed callback */
static void joystick_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t val)
{
	printk("CCC changed: val %u\n", val);
	remote_ready = (val == BT_GATT_CCC_NOTIFY);
	
	if (remote_ready) {
		printk("Remote device ready to receive notifications\n");
	}
}

/* GATT Service Definition */
BT_GATT_SERVICE_DEFINE(joystick_svc,
	BT_GATT_PRIMARY_SERVICE(&joystick_svc_uuid.uuid),
	BT_GATT_CHARACTERISTIC(&joystick_chr_uuid.uuid, 
	                       BT_GATT_CHRC_NOTIFY,
	                       BT_GATT_PERM_NONE, 
	                       NULL, NULL, NULL),
	BT_GATT_CCC(joystick_ccc_cfg_changed,
	            BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

/* Connection callback */
static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		printk("Connection failed (err 0x%02x)\n", err);
		return;
	}

	printk("Connected\n");

	if (!default_conn) {
		default_conn = bt_conn_ref(conn);
	}

	is_connected = true;
	remote_ready = false;  /* Venter på at remote aktiverer notifikasjoner */
}

/* Disconnection callback */
static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected (reason 0x%02x)\n", reason);

	if (default_conn) {
		bt_conn_unref(default_conn);
		default_conn = NULL;
	}

	is_connected = false;
	remote_ready = false;
}

/* Connection callbacks */
static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
};

/* Start advertising */
void ble_connect(void)
{
	int err;

	if (is_connected) {
		printk("Already connected\n");
		return;
	}

	err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		printk("Advertising failed to start (err %d)\n", err);
		return;
	}

	printk("Advertising successfully started\n");
	printk("Waiting for connection...\n");
}

/* Stop advertising */
void ble_cancel_connect(void)
{
	int err;

	if (is_connected) {
		/* Disconnect if connected */
		bt_conn_disconnect(default_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	} else {
		/* Stop advertising */
		err = bt_le_adv_stop();
		if (err) {
			printk("Failed to stop advertising (err %d)\n", err);
		} else {
			printk("Advertising stopped\n");
		}
	}
}

/* Initialize Bluetooth */
void ble_init(void)
{
	int err;

	printk("Initializing Bluetooth...\n");

	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	/* Register connection callbacks */
	bt_conn_cb_register(&conn_callbacks);

	/* Store reference to local GATT characteristic */
	local_attr = &joystick_svc.attrs[1];

	printk("Joystick BLE service ready\n");
	printk("Device name: %s\n", CONFIG_BT_DEVICE_NAME);
}