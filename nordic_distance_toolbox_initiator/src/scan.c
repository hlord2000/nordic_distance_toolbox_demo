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

// TODO: Add chaging RBG LED color based on ranging target based on RNG
// change ranging with button press

static void scan_filter_match(struct bt_scan_device_info *device_info,
                       struct bt_scan_filter_match *filter_match,
                       bool connectable)
{
    char addr[BT_ADDR_SIZE];
    bt_addr_to_str(&device_info->recv_info->addr, addr, BT_ADDR_SIZE);
    uint64_t addr_int = 0;
    for (int i = 0; i < BT_ADDR_SIZE; i++) {
        addr_int = addr_int << 8;
        addr_int += addr[i];
    }

    struct dm_request req;
    req.role = DM_ROLE_INITIATOR;
    req.ranging_mode = DM_RANGING_MODE_MCPD;
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

    struct peer *p = get_peer(addr_int);

    req.rng_seed = p->rng_seed;
    req.start_delay_us = 0;
    req.extra_window_time_us = 100;
    int err = dm_request_add(&req);
}

bool validate_ndt_manufacturer_data(uint8_t *data, uint8_t data_len) {
    struct adv_mfg_data *mfg_data;
    if (data_len != sizeof(*mfg_data)) {
        return false;
    }

    mfg_data = (struct adv_mfg_data *)data;

    if (mfg_data->support_dm_code == 0xD157A9CE) {
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
                err = create_peer(addr, mfg_data.rng_seed);
                if (err) {
                    LOG_ERR("Failed to create peer (err %d)\n", err);
                }
            }
            break;
        case BT_DATA_UUID128_ALL:
            struct bt_uuid_128 uuid;

            memcpy(uuid.val, data->data, BT_UUID_SIZE_128);
            uuid.uuid.type = BT_UUID_TYPE_128;

            set_filter_uuid(&uuid);
            err = uuid_set_peer(addr, uuid);
            if (err) {
                LOG_ERR("Failed to set peer uuid (err %d)\n", err);
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
    char addr[BT_ADDR_SIZE];
    bt_addr_to_str(&device_info->recv_info->addr, addr, BT_ADDR_SIZE);
    uint64_t addr_int = 0;
    for (int i = 0; i < BT_ADDR_SIZE; i++) {
        addr_int = addr_int << 8;
        addr_int += addr[i];
    }

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