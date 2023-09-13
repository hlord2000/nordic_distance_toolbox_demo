#include "advertise.h"

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <bluetooth/gatt_dm.h>

#include <dm.h>

LOG_MODULE_REGISTER(advertise, LOG_LEVEL_DBG);

#define DEVICE_NAME             CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN         (sizeof(DEVICE_NAME) - 1)

#define COMPANY_CODE 0x0059
#define SUPPORT_DM_CODE 0xFF55AA5A

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
			     BT_LE_ADV_OPT_NOTIFY_SCAN_REQ,
                 BT_GAP_ADV_SLOW_INT_MIN,
                 BT_GAP_ADV_SLOW_INT_MAX,
			     NULL);


struct bt_le_adv_param *adv_param = &adv_param_noconn;

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

#define BT_UUID_NDT BT_UUID_128_ENCODE(0x65abd3db, 0x1c17, 0x485e, 0xa74c, 0xc180089b4538)

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_MANUFACTURER_DATA, (unsigned char *)&mfg_data, sizeof(mfg_data)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NDT),
};


static void adv_scanned_cb(struct bt_le_ext_adv *adv, struct bt_le_ext_adv_scanned_info *info) {
    struct dm_request req;

    bt_addr_le_copy(&req.bt_addr, info->addr);
    req.role = DM_ROLE_REFLECTOR;
    req.ranging_mode = DM_RANGING_MODE_MCPD;

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
    req.rng_seed = 1234;
    req.start_delay_us = 0;
    req.extra_window_time_us = 0;

    dm_request_add(&req);
}

const static struct bt_le_ext_adv_cb adv_cb = {
	.scanned = adv_scanned_cb,
};


int advertise_init(void) {
    int err;
    mfg_data.company_code = COMPANY_CODE;
    mfg_data.support_dm_code = SUPPORT_DM_CODE;
    mfg_data.rng_seed = 0x12345678;

    struct bt_le_ext_adv_start_param ext_adv_start_param = {0};

    if (adv) {
		err = bt_le_ext_adv_stop(adv);
		if (err) {
			printk("Failed to stop extended advertising  (err %d)\n", err);
			return err;
		}
			err = bt_le_ext_adv_delete(adv);
		if (err) {
			printk("Failed to delete advertising set  (err %d)\n", err);
			return err;
		}
	}

	err = bt_le_ext_adv_create(adv_param, &adv_cb, &adv);
	if (err) {
		printk("Failed to create advertising set (err %d)\n", err);
		return err;
	}

	err = bt_le_ext_adv_set_data(adv, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		printk("Failed setting adv data (err %d)\n", err);
		return err;
	}

	err = bt_le_ext_adv_start(adv, &ext_adv_start_param);
	if (err) {
		printk("Failed to start extended advertising  (err %d)\n", err);
		return err;
	}

	return err;
}
