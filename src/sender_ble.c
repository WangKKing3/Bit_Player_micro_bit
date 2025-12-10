#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>

#include "sender_ble.h"
#include "motor_types.h"


#define DRIVE_SVC_UUID \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0)
#define DRIVE_CHR_UUID \
    BT_UUID_128_ENCODE(0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1)

static struct bt_uuid_128 svc_uuid = BT_UUID_INIT_128(DRIVE_SVC_UUID);
static struct bt_uuid_128 chr_uuid = BT_UUID_INIT_128(DRIVE_CHR_UUID);

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, DRIVE_SVC_UUID),
};

static struct bt_conn *conn;
static const struct bt_gatt_attr *notify_attr;
static bool subscribed;
static bool connected;

// GATT characteristic write callback
void ble_send_direction(Motor_direction dir, uint8_t speed) {
    if (!connected || !subscribed) {
        return;
    }

    struct drive_packet packet = {
        .direction = (uint8_t)dir,
        .speed = speed
    };

    bt_gatt_notify(conn, notify_attr, &packet, sizeof(packet));
}

// Bluetooth connection callbacks
bool ble_is_ready(void) {
    return (conn != NULL && subscribed);
}

// Bluetooth connection callbacks
static void ccc_changed(const struct bt_gatt_attr *attr, uint16_t val)
{
    subscribed = (val == BT_GATT_CCC_NOTIFY);
    printk("Subscribed: %s\n", subscribed ? "yes" : "no");
}

BT_GATT_SERVICE_DEFINE(drive_svc,
    BT_GATT_PRIMARY_SERVICE(&svc_uuid.uuid),
    BT_GATT_CHARACTERISTIC(&chr_uuid.uuid,
                           BT_GATT_CHRC_NOTIFY,
                           BT_GATT_PERM_NONE,
                           NULL, NULL, NULL),
    BT_GATT_CCC(ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

static void reconnect_work_fn(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(reconnect_work, reconnect_work_fn);

static void reconnect_work_fn(struct k_work *work)
{
    if (!connected) {
        int err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
        if (err && err != -EALREADY) {
            k_work_schedule(&reconnect_work, K_SECONDS(1));
        }
    }
}

static void on_connected(struct bt_conn *c, uint8_t err)
{
    if (err) {
        printk("Connect failed: %d\n", err);
        k_work_schedule(&reconnect_work, K_MSEC(200));
        return;
    }
    printk("Connected\n");
    conn = bt_conn_ref(c);
    connected = true;
    subscribed = false;
}


static void on_disconnected(struct bt_conn *c, uint8_t reason)
{
    printk("Disconnected: 0x%02x\n", reason);
    if (conn) {
        bt_conn_unref(conn);
        conn = NULL;
    }
    connected = false;
    subscribed = false;
    k_work_schedule(&reconnect_work, K_MSEC(100));
}

static struct bt_conn_cb conn_cb = {
    .connected = on_connected,
    .disconnected = on_disconnected,
};

void ble_connect(void)
{
    if (connected) return;
    int err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        printk("Adv failed: %d\n", err);
        k_work_schedule(&reconnect_work, K_MSEC(500));
    } else {
        printk("Advertising...\n");
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
    bt_conn_cb_register(&conn_cb);
    notify_attr = &drive_svc.attrs[1];
}