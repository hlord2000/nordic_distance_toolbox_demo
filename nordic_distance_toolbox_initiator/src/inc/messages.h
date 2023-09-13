#include <zephyr/bluetooth/bluetooth.h>

struct dm_data {
	float distance;
	char addr[BT_ADDR_LE_STR_LEN];
};