//Joystick_ble.c - SIMPLIFIED VERSION

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <string.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>

#include "joystick_ble.h"

#define JOYSTICK_SVC_UUID \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)
#define JOYSTICK_CHR_UUID \
	BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1)

static struct bt_uuid_128 joystick_svc_uuid = BT_UUID_INIT_128(JOYSTICK_SVC_UUID);
static struct bt_uuid_128 joystick_chr_uuid = BT_UUID_INIT_128(JOYSTICK_CHR_UUID);

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, JOYSTICK_SVC_UUID),
};

static struct bt_conn *default_conn;
static const struct bt_gatt_attr *local_attr;
static bool remote_ready;
static bool is_connected;

/* Joystick data */
struct joystick_data {
	int16_t x_pos;
	int16_t y_pos;
	uint8_t buttons;
} __packed;

/* Send data - enkel versjon */
void ble_send_joystick_data(int16_t x_pos, int16_t y_pos, uint8_t buttons)
{
	if (!default_conn || !remote_ready) return;

	struct joystick_data data = {
		.x_pos = x_pos,
		.y_pos = y_pos,
		.buttons = buttons,
	};

	int err = bt_gatt_notify(default_conn, local_attr, &data, sizeof(data));
	if (err && err != -ENOMEM && err != -ENOTCONN) {
		printk("Notify err %d\n", err);
	}
}

bool ble_is_ready(void)
{
	return (default_conn != NULL && remote_ready);
}

static void joystick_ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t val)
{
	remote_ready = (val == BT_GATT_CCC_NOTIFY);
	printk("CCC: %s\n", remote_ready ? "subscribed" : "unsubscribed");
}

BT_GATT_SERVICE_DEFINE(joystick_svc,
	BT_GATT_PRIMARY_SERVICE(&joystick_svc_uuid.uuid),
	BT_GATT_CHARACTERISTIC(&joystick_chr_uuid.uuid, 
	                       BT_GATT_CHRC_NOTIFY,
	                       BT_GATT_PERM_NONE, 
	                       NULL, NULL, NULL),
	BT_GATT_CCC(joystick_ccc_cfg_changed,
	            BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

/* Auto-reconnect work */
static void restart_adv_work(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(adv_work, restart_adv_work);

static void restart_adv_work(struct k_work *work)
{
	if (!is_connected) {
		int err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
		if (err && err != -EALREADY) {
			printk("Adv retry in 1s\n");
			k_work_schedule(&adv_work, K_SECONDS(1));
		} else {
			printk("Advertising...\n");
		}
	}
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		printk("Connect failed: 0x%02x\n", err);
		k_work_schedule(&adv_work, K_MSEC(200));
		return;
	}

	printk("Connected\n");
	if (!default_conn) {
		default_conn = bt_conn_ref(conn);
	}
	is_connected = true;
	remote_ready = false;
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	printk("Disconnected: 0x%02x\n", reason);

	if (default_conn) {
		bt_conn_unref(default_conn);
		default_conn = NULL;
	}
	is_connected = false;
	remote_ready = false;

	/* Rask reconnect */
	k_work_schedule(&adv_work, K_MSEC(100));
}

static struct bt_conn_cb conn_callbacks = {
	.connected = connected,
	.disconnected = disconnected,
};

void ble_connect(void)
{
	if (is_connected) return;

	int err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
	if (err) {
		printk("Adv failed: %d\n", err);
		k_work_schedule(&adv_work, K_MSEC(500));
		return;
	}
	printk("Advertising started\n");
}

void ble_cancel_connect(void)
{
	k_work_cancel_delayable(&adv_work);
	if (is_connected && default_conn) {
		bt_conn_disconnect(default_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	} else {
		bt_le_adv_stop();
	}
}

void ble_init(void)
{
	int err = bt_enable(NULL);
	if (err) {
		printk("BT init failed: %d\n", err);
		return;
	}
	printk("Bluetooth ready\n");
	bt_conn_cb_register(&conn_callbacks);
	local_attr = &joystick_svc.attrs[1];
}