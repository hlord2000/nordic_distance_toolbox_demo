#include "advertise.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <bluetooth/gatt_dm.h>

#include <zephyr/random/rand32.h>

#include <dm.h>

LOG_MODULE_REGISTER(advertise, LOG_LEVEL_DBG);

#define DEVICE_NAME             "Thingy" 
#define DEVICE_NAME_LEN         (sizeof(DEVICE_NAME) - 1)

#define COMPANY_CODE 0x0059
#define SUPPORT_DM_CODE 0xD157A9CE

static struct bt_le_ext_adv *adv;

struct adv_mfg_data {
	uint16_t company_code;	    /* Company Identifier Code. */
	uint32_t support_dm_code;   /* To identify the device that supports distance measurement. */
	uint32_t rng_seed;          /* Random seed used for generating hopping patterns. */
} __packed;

static struct adv_mfg_data mfg_data;

struct bt_le_adv_param adv_param_noconn =
	BT_LE_ADV_PARAM_INIT(BT_LE_ADV_OPT_USE_IDENTITY |
				BT_LE_ADV_OPT_SCANNABLE |
				BT_LE_ADV_OPT_NOTIFY_SCAN_REQ |
				BT_LE_ADV_OPT_CONNECTABLE,
				BT_GAP_ADV_FAST_INT_MIN_2,
				BT_GAP_ADV_FAST_INT_MAX_2,
				NULL);


struct bt_le_adv_param *adv_param = &adv_param_noconn;

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_MANUFACTURER_DATA, (unsigned char *)&mfg_data, sizeof(mfg_data)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

#define BT_UUID_NULL BT_UUID_128_ENCODE(0, 0, 0, 0, 0)

static struct bt_data sd[] = {
	BT_DATA(BT_DATA_MANUFACTURER_DATA, (unsigned char *)&mfg_data, sizeof(mfg_data)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NULL),
};


static void adv_scanned_cb(struct bt_le_ext_adv *adv, struct bt_le_ext_adv_scanned_info *info) {
    struct dm_request req;

    bt_addr_le_copy(&req.bt_addr, info->addr);
    req.role = DM_ROLE_REFLECTOR;
	#ifdef CONFIG_MCPD_DISTANCE
    req.ranging_mode = DM_RANGING_MODE_MCPD;
	#endif

	#ifdef CONFIG_RTT_DISTANCE
	req.ranging_mode = DM_RANGING_MODE_RTT;
	#endif

    /* We need to make sure that we only initiate a ranging to a single peer.
        * A scan response from this device can be received by multiple peers which can
        * all start a ranging at the same time as a consequence. To prevent this,
        * we need to make sure that we set a per-peer random as the random seed.
        * This helps the ranging library to avoid interference from other devices
        * trying to range at the same time.
        *
        * This means that the initiator and the reflector need to set the same value
        * for the random seed.
        */
    bt_addr_le_copy(&req.bt_addr, info->addr);
    req.rng_seed = mfg_data.rng_seed;
    req.start_delay_us = 0;
    req.extra_window_time_us = 0;

    dm_request_add(&req);
}

static void connected(struct bt_conn *conn, uint8_t err) {
	char addr[BT_ADDR_LE_STR_LEN];

	if (err) {
		LOG_ERR("Connection failed (err %u)\n", err);
		return;
	}

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("Connected %s\n", addr);
}

static void disconnected(struct bt_conn *conn, uint8_t reason) {
	LOG_INF("Disconnected (reason %u)", reason);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected, };
const static struct bt_le_ext_adv_cb adv_cb = {
	.scanned = adv_scanned_cb
};


int advertise_init(void) {
    int err;

	int uuid[16] = {0};
	sys_csrand_get(uuid, 16);

	sd[1].data = uuid;
	sd[1].data_len = 16;
	sd[1].type = BT_DATA_UUID128_ALL;

    mfg_data.company_code = COMPANY_CODE;
    mfg_data.support_dm_code = SUPPORT_DM_CODE;
	sys_csrand_get(&mfg_data.rng_seed, sizeof(mfg_data.rng_seed));

    struct bt_le_ext_adv_start_param ext_adv_start_param = {0};

    if (adv) {
		err = bt_le_ext_adv_stop(adv);
		if (err) {
			LOG_ERR("Failed to stop extended advertising  (err %d)\n", err);
			return err;
		}
			err = bt_le_ext_adv_delete(adv);
		if (err) {
			LOG_ERR("Failed to delete advertising set  (err %d)\n", err);
			return err;
		}
	}

	err = bt_le_ext_adv_create(adv_param, &adv_cb, &adv);
	if (err) {
		LOG_ERR("Failed to create advertising set (err %d)\n", err);
		return err;
	}

	err = bt_le_ext_adv_set_data(adv, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("Failed setting adv data (err %d)\n", err);
		return err;
	}

	err = bt_le_filter_accept_list_clear();
	if (err) {
		LOG_ERR("Failed to clear accept list (err %d)\n", err);
	}

	err = bt_le_ext_adv_start(adv, &ext_adv_start_param);
	if (err) {
		LOG_ERR("Failed to start extended advertising  (err %d)\n", err);
		return err;
	}

	return err;
}
