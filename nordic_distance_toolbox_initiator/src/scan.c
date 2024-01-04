// Kernel includes
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/random/rand32.h>

// BLE includes
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <bluetooth/gatt_dm.h>
#include <bluetooth/scan.h>

#include <dm.h>

#include <peer.h>

struct adv_mfg_data {
	uint16_t company_code;	    /* Company Identifier Code. */
	uint32_t support_dm_code;   /* To identify the device that supports distance measurement. */
	uint32_t rng_seed;          /* Random seed used for generating hopping patterns. */
} __packed;

// TODO add log level kconfig
LOG_MODULE_REGISTER(scan, LOG_LEVEL_DBG);

static uint64_t bt_addr_to_int(const bt_addr_le_t *addr) {
    uint64_t addr_int = 0;
    for (int i = 0; i < BT_ADDR_SIZE; i++) {
        addr_int = addr_int << 8;
        addr_int += addr->a.val[i];
    }
    return addr_int;
}

static void scan_filter_match(struct bt_scan_device_info *device_info,
                       struct bt_scan_filter_match *filter_match,
                       bool connectable)
{
    uint64_t addr_int = bt_addr_to_int(&device_info->recv_info->addr);

    if (!filter_match->uuid.match) {
        return;
    }

    static struct bt_uuid_128 *uuid;
    if (filter_match->uuid.uuid[0]->type == 2) {
        uuid = (struct bt_uuid_128 *)(filter_match->uuid.uuid[0]);
    }
    else {
        return;
    }

    struct peer *p = get_peer(uuid);
    if (p == NULL) {
        return;
    }

    uint32_t current_time = k_uptime_get();
    if (p->timestamp + CONFIG_DM_PEER_DELAY_MS < current_time) {
        p->timestamp = current_time;
    }
    else {
        return;
    }

    struct dm_request req;
    req.role = DM_ROLE_INITIATOR;

    if (p->ranging_mode == RANGING_MODE_MCPD) {
        req.ranging_mode = DM_RANGING_MODE_MCPD;
    }
    else if (p->ranging_mode == RANGING_MODE_RTT) {
        req.ranging_mode = DM_RANGING_MODE_RTT;
    }
    else {
        return;
    }

   /* We need to make sure that we only initiate a ranging to a single peer.
    * A scan response that is received by this device can be received by
    * multiple other devices which can all start a ranging at the same time
    * as a consequence. To prevent this, we need to make sure that we set a
    * per-peer random as the random seed. This helps the ranging library to
    * avoid interference from other devices trying to range at the same time.
    *
    * This means that the initiator and the reflector need to set the same
    * value for the random seed.
    */

    bt_addr_le_copy(&req.bt_addr, device_info->recv_info->addr);

    req.rng_seed = p->rng_seed;
    req.start_delay_us = 0;
    req.extra_window_time_us = 0;

    int err = dm_request_add(&req);
    if (err) {
        LOG_ERR("Failed to add request (err %d)\n", err);
    }
}

bool validate_ndt_manufacturer_data(uint8_t *data, uint8_t data_len) {
    struct adv_mfg_data *mfg_data;

    mfg_data = (struct adv_mfg_data *)data;


    if (mfg_data->support_dm_code == 0x0D17A9CE) {
        return true;
    }

    if (mfg_data->support_dm_code == 0x1D17A9CE) {
        return true;
    }
    return false;
}

static void set_filter_uuid(struct bt_uuid_128 *uuid) {
    LOG_INF("Adding filter");

    int err;
    err = bt_scan_stop();
    if (err) {
        LOG_ERR("Scanning failed to stop (err %d)\n", err);
        return err;
    }

	err = bt_scan_filter_add(BT_SCAN_FILTER_TYPE_UUID, uuid);
	if (err) {
		LOG_ERR("Scanning filters cannot be set (err %d)\n", err);
		return err;
	}

	err = bt_scan_filter_enable(BT_SCAN_UUID_FILTER, false);
	if (err) {
		LOG_ERR("Filters cannot be turned on (err %d)\n", err);
		return err;
	}

    err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
    if (err) {
        LOG_ERR("Scanning failed to start (err %d)\n", err);
        return err;
    }
}

static bool ndt_supported(struct bt_data *data, void *user_data) {
    int err;
    volatile uint64_t addr = *(uint64_t *)user_data;
    switch(data->type) {
        case BT_DATA_MANUFACTURER_DATA:
            struct adv_mfg_data mfg_data = *(struct adv_mfg_data *)data->data;
            if (validate_ndt_manufacturer_data(data->data, data->data_len)) {
                err = create_peer(addr, mfg_data.rng_seed, mfg_data.support_dm_code);
                if (err) {
                    LOG_ERR("Failed to create peer (err %d)\n", err);
                }
            }
            break;
        case BT_DATA_UUID128_ALL:
            struct bt_uuid_128 uuid;

            memcpy(uuid.val, data->data, BT_UUID_SIZE_128);
            uuid.uuid.type = BT_UUID_TYPE_128;

            err = uuid_set_peer(addr, uuid);
            if (err) {
                LOG_ERR("Failed to set peer uuid (err %d)\n", err);
                LOG_ERR("UUID: %s", bt_uuid_str(&uuid));
            } 
            else {
                set_filter_uuid(&uuid);
            }
            break;
        default:
            return true;
    }
    return true;
}

static void scan_filter_no_match(struct bt_scan_device_info *device_info,
                          bool connectable)
{
    uint64_t addr_int = bt_addr_to_int(&device_info->recv_info->addr);

    switch (device_info->recv_info->adv_type) {
        case BT_GAP_ADV_TYPE_SCAN_RSP:
        case BT_GAP_ADV_TYPE_EXT_ADV:
            bt_data_parse(device_info->adv_data, ndt_supported, &addr_int);
            break;
        default:
            break;
    }
}

BT_SCAN_CB_INIT(scan_cb, scan_filter_match, scan_filter_no_match, NULL, NULL);

// Scan without sending scan requests automatically
static struct bt_le_scan_param scan_param = {
    .type = BT_LE_SCAN_TYPE_ACTIVE,
    .interval = BT_GAP_SCAN_FAST_INTERVAL,
    .window = BT_GAP_SCAN_FAST_WINDOW,
    .options = BT_LE_SCAN_OPT_FILTER_DUPLICATE,
    .timeout = 0,
};

int scan_init(void)
{
	int err;

	struct bt_scan_init_param scan_init = {
		.connect_if_match = false,
		.scan_param = &scan_param,
		.conn_param = BT_LE_CONN_PARAM_DEFAULT
	};

	bt_scan_init(&scan_init);
	bt_scan_cb_register(&scan_cb);

    err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
    if (err) {
        LOG_ERR("Scanning failed to start (err %d)\n", err);
        return err;
    }

    return 0;
}