#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#include <zephyr/bluetooth/bluetooth.h>

#include <dm.h>

#include <scan.h>
#include <messages.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

ZBUS_CHAN_DECLARE(dm_chan);

void data_ready(struct dm_result *result)
{
	if (!result) {
		return;
	}

	const char *quality[DM_QUALITY_NONE + 1] = {"ok", "poor", "do not use", "crc fail", "none"};
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(&result->bt_addr, addr, sizeof(addr));

	struct dm_data dm_data = {0};
	dm_data.distance = result->dist_estimates.mcpd.best;
	memcpy(dm_data.addr, addr, sizeof(addr));

	LOG_INF("\nMeasurement result:\n");
	LOG_INF("\tAddr: %s\n", addr);
	LOG_INF("\tQuality: %s\n", quality[result->quality]);

	LOG_INF("\tDistance estimates: ");
	LOG_INF("mcpd: ifft=%.2f phase_slope=%.2f rssi_openspace=%.2f best=%.2f\n",
		result->dist_estimates.mcpd.ifft,
		result->dist_estimates.mcpd.phase_slope,
		result->dist_estimates.mcpd.rssi_openspace,
		result->dist_estimates.mcpd.best);
	zbus_chan_pub(&dm_chan, &dm_data, K_MSEC(500));
}

static struct dm_cb dm_cb = {
	.data_ready = data_ready,
};


int main(void)
{
	int err;

	struct dm_init_param init_param;
	init_param.cb = &dm_cb;

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth failed to start (err %d)\n", err);
	}
	LOG_INF("Bluetooth initialized\n");

	err = scan_init();
	if (err) {
		LOG_ERR("Scanning failed to start (err %d)\n", err);
	}
	LOG_INF("Scanning initialized\n");

	err = dm_init(&init_param);
	if (err) {
		LOG_ERR("Distance measurement failed to start (err %d)\n", err);
	}
	LOG_INF("Distance measurement initialized\n");

	while (true) {
		//LOG_INF("Keep on truckin...");
		k_sleep(K_MSEC(5000));
	}

}
