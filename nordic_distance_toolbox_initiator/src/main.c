#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/hash_function.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/zbus/zbus.h>

#include <zephyr/bluetooth/bluetooth.h>

#include <dm.h>

#include <scan.h>
#include <messages.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

ZBUS_CHAN_DECLARE(dm_chan);

#ifdef CONFIG_BOARD_THINGY53_NRF5340_CPUAPP
static const struct pwm_dt_spec red_pwm_led =
	PWM_DT_SPEC_GET(DT_ALIAS(pwm_led0));
static const struct pwm_dt_spec green_pwm_led =
	PWM_DT_SPEC_GET(DT_ALIAS(pwm_led1));
static const struct pwm_dt_spec blue_pwm_led =
	PWM_DT_SPEC_GET(DT_ALIAS(pwm_led2));

#define PWM_PERIOD_USEC 2000

void set_led_color(uint32_t rgba_color) {
	uint8_t red = (rgba_color >> 24) & 0xFF;
	uint8_t green = (rgba_color >> 16) & 0xFF;
	uint8_t blue = (rgba_color >> 8) & 0xFF;
	uint8_t alpha = rgba_color & 0xFF;

	uint32_t red_duty = PWM_USEC((PWM_PERIOD_USEC * red * alpha) / (0xFF * 0xFF));
	uint32_t green_duty = PWM_USEC((PWM_PERIOD_USEC * green * alpha) / (0xFF * 0xFF));
	uint32_t blue_duty = PWM_USEC((PWM_PERIOD_USEC * blue * alpha) / (0xFF * 0xFF));

	uint32_t period = PWM_USEC(PWM_PERIOD_USEC);

	pwm_set_dt(&red_pwm_led, period, red_duty);	
	pwm_set_dt(&green_pwm_led, period, green_duty);
	pwm_set_dt(&blue_pwm_led, period, blue_duty);
}

static uint32_t hash_to_color(uint32_t color_hash) {
	uint8_t red = (color_hash >> 24) & 0xFF;
	uint8_t green = (color_hash >> 16) & 0xFF;
	uint8_t blue = (color_hash >> 8) & 0xFF;
	uint8_t alpha = 0xFF;

	if (red > 120 && green > 120 && blue > 120) {
        red -= 50; green -= 50; blue -= 50; // Reduce brightness
    } else if (red < 55 && green < 55 && blue < 55) {
        red += 50; green += 50; blue += 50; // Increase brightness
    }
	
	return (red << 24) | (green << 16) | (blue << 8) | alpha;
}

static void set_color_by_dist(char *addr, size_t addr_len) {
	// Get my address and convert to color:
	const char addr_local_str[BT_ADDR_LE_STR_LEN];
	memcpy(addr_local_str, addr, 17);

	LOG_INF("Input: %s", addr_local_str);
	uint32_t color_hash = sys_hash32_murmur3(addr_local_str, 17);
	LOG_INF("Hash: %x", color_hash);
	uint32_t color = hash_to_color(color_hash);
	LOG_INF("Color: %x", color);
	set_led_color(color);
}
#endif

void data_ready(struct dm_result *result)
{
	if (!result) {
		return;
	}

	const char *quality[DM_QUALITY_NONE + 1] = {"ok", "poor", "do not use", "crc fail", "none"};
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(&result->bt_addr, addr, sizeof(addr));

	struct dm_data dm_data = {0};
	dm_data.distance = result->dist_estimates.rtt.rtt;
	memcpy(dm_data.addr, addr, sizeof(addr));
	
	set_color_by_dist(addr, sizeof(addr));

	LOG_INF("\nMeasurement result:\n");
	LOG_INF("\tAddr: %s\n", addr);
	LOG_INF("\tQuality: %s\n", quality[result->quality]);

	LOG_INF("\tDistance estimates: ");
	LOG_INF("rtt: %.2f m, ", result->dist_estimates.rtt.rtt);
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
