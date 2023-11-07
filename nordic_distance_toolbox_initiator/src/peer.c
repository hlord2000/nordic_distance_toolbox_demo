#include <peer.h>
#include <string.h>
#include <stddef.h>
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/uuid.h>

#define NUM_PEERS CONFIG_BT_SCAN_UUID_CNT

struct peer peer_array[NUM_PEERS] = {0};

int create_peer(uint64_t addr_int, uint32_t rng_seed) {
    for (int i = 0; i < NUM_PEERS; i++) {
        if (peer_array[i].is_active == false) {
            peer_array[i].addr_int = addr_int;
            peer_array[i].timestamp = 0;
            peer_array[i].rng_seed = rng_seed;

            return 0;
        }
    }
    return -ENOMEM;
}

int uuid_set_peer(uint64_t addr_int, struct bt_uuid_128 uuid) {
    for (int i = 0; i < NUM_PEERS; i++) {
        if (peer_array[i].addr_int == addr_int) {
            peer_array[i].uuid = uuid;
            memcpy(peer_array[i].uuid.val, uuid.val, BT_UUID_SIZE_128);
            peer_array[i].uuid.uuid.type = BT_UUID_TYPE_128;

            peer_array[i].is_active = true;
            return 0;
        }
    }
    return -ENOMEM;
}

int remove_peer(uint64_t addr_int) {
    for (int i = 0; i < NUM_PEERS; i++) {
        if (peer_array[i].is_active == true && peer_array[i].addr_int == addr_int) {
            peer_array[i].is_active = false;
            return 0;
        }
    }
    return -ENOENT;
}

struct peer * get_peer(struct bt_uuid_128 *uuid) {
    int cmp;
    for (int i = 0; i < NUM_PEERS; i++) {
        if (peer_array[i].is_active == true) {
            cmp = strncmp(peer_array[i].uuid.val, uuid->val, BT_UUID_SIZE_128);
            if (cmp == 0) {
                return &peer_array[i];
            }
        }
    }
    return NULL;
}