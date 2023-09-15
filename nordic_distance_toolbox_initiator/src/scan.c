// Kernel includes
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/hash_map.h>
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

struct adv_mfg_data {
	uint16_t company_code;	    /* Company Identifier Code. */
	uint32_t support_dm_code;   /* To identify the device that supports distance measurement. */
	uint32_t rng_seed;          /* Random seed used for generating hopping patterns. */
} __packed;

struct peer {
    uint32_t rng_seed;
    bool filter_set;
};

SYS_HASHMAP_DEFINE_STATIC(peer_map);

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

    uint64_t *peer_map_val;
    if(sys_hashmap_get(&peer_map, addr_int, peer_map_val)) {
        req.rng_seed = *(uint32_t *)peer_map_val;
    }
    else {
        return;
    }

    req.start_delay_us = 0;
    req.extra_window_time_us = 0;
    int err = dm_request_add(&req);
    if (err) {
        LOG_ERR("Failed to add ranging request (err %d)\n", err);
    }
    else {
        LOG_INF("Added ranging request");
    }
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

static void set_filter_uuid(uint8_t *uuid_128) {
    LOG_INF("Adding filter");
    int err;

    struct bt_uuid_128 uuid;
    uuid.uuid.type = BT_UUID_TYPE_128;
    memcpy(uuid.val, uuid_128, 16);

    err = bt_scan_stop();
    if (err) {
        LOG_ERR("Scanning failed to stop (err %d)\n", err);
        return err;
    }

	err = bt_scan_filter_add(BT_SCAN_FILTER_TYPE_UUID, &uuid);
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
    volatile uint64_t addr = *(uint64_t *)user_data;
    switch(data->type) {
        case BT_DATA_MANUFACTURER_DATA:
            // If the manufacturer data is valid and the peer is not already in the peer map:
            struct adv_mfg_data mfg_data = *(struct adv_mfg_data *)data->data;
            if (validate_ndt_manufacturer_data(data->data, data->data_len)) {
                if (!sys_hashmap_contains_key(&peer_map, addr)) {
                    // Add the peer to the peer map where key is addr and value is rng_seed
                    struct peer *peer = k_malloc(sizeof(struct peer));
                    memset(peer, 0, sizeof(struct peer));

                    peer->rng_seed = mfg_data.rng_seed;
                    peer->filter_set = false;

                    int err = sys_hashmap_insert(&peer_map, addr, peer, NULL);
                    if (err < 0) {
                        LOG_ERR("Failed to insert peer into peer map (err %d)\n", err);
                    }
                }
            }
            else {
                LOG_INF("Not adding peer to peer map");
            }
            break;
        case BT_DATA_UUID128_ALL:
            uint64_t peer_map_val;
            if (sys_hashmap_get(&peer_map, addr, peer_map_val)) {
                // Reinterpret the peer_map_val as a pointer to a peer struct
                struct peer *peer_temp_p = (struct peer *)peer_map_val;

                struct peer peer_temp = *peer_temp_p;

                peer_temp.filter_set = true;
                set_filter_uuid(data->data);
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