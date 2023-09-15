#ifndef PEER_H__
#define PEER_H__
#include <stdint.h>
#include <stdbool.h>

#include <zephyr/bluetooth/uuid.h>

struct peer {
    uint32_t rng_seed;
    uint64_t addr_int;
    struct bt_uuid_128 uuid;
    bool filter_set;
    bool is_active;
};

#endif