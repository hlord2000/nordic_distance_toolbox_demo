#ifndef PEER_H__
#define PEER_H__
#include <stdint.h>
#include <stdbool.h>

#include <zephyr/bluetooth/uuid.h>

struct peer {
    uint32_t rng_seed;
    uint64_t addr_int;
    uint32_t color;
    struct bt_uuid_128 uuid;
    bool filter_set;
    bool is_active;
    uint32_t timestamp;
};

struct peer *get_peer(uint64_t addr_int);

int uuid_set_peer(uint64_t addr_int, struct bt_uuid_128 uuid);

int remove_peer(uint64_t addr_int);

int create_peer(uint64_t addr_int, uint32_t rng_seed);

#endif