// Kernel includes
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

// BLE includes
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <bluetooth/gatt_dm.h>
#include <bluetooth/scan.h>

#include <dm.h>

// TODO add log level kconfig
LOG_MODULE_REGISTER(scan, LOG_LEVEL_DBG);

bool print_data(struct bt_data *data, void *user_data)
{
    switch(data->type) {
        case BT_DATA_MANUFACTURER_DATA:
            LOG_INF("Manufacturer data");
            break;
        case BT_DATA_UUID128_ALL:
        case BT_DATA_UUID128_SOME:
            LOG_INF("128-bit UUID");
            break;
        default:
            return true;
    }
    LOG_HEXDUMP_INF(data->data, data->data_len, "AD data");
    return true;
}

static void scan_filter_match(struct bt_scan_device_info *device_info,
                       struct bt_scan_filter_match *filter_match,
                       bool connectable)
{
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
    req.rng_seed = 1234;
    req.start_delay_us = 0;
    req.extra_window_time_us = 0;

    dm_request_add(&req);
}

static void scan_filter_no_match(struct bt_scan_device_info *device_info,
                          bool connectable)
{
    LOG_INF("No filter match for device");
    // Need to detect if this is a scan response packet, if so and the mfg data matches, create filter
    // for UUID
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

#if 1
    struct bt_uuid_128 uuid_128 = BT_UUID_INIT_128(
        BT_UUID_128_ENCODE(0x65abd3db, 0x1c17, 0x485e, 0xa74c, 0xc180089b4538));
#endif

#if 0
    struct bt_uuid_128 uuid_128 = BT_UUID_INIT_128(
        BT_UUID_128_ENCODE(0x65abd3dc, 0x1c17, 0x485e, 0xa74c, 0xc180089b4560));  
#endif

	err = bt_scan_filter_add(BT_SCAN_FILTER_TYPE_UUID, &uuid_128);
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

    return 0;
}