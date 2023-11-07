#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/hash_function.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/zbus/zbus.h>

#include <zephyr/bluetooth/bluetooth.h>

#include <dm.h>

#include <color.h>
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
static const struct gpio_dt_spec dm_ranging_complete_dev =
	GPIO_DT_SPEC_GET(DT_NODELABEL(dm_ranging_complete), gpios);

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
#endif

void data_ready(struct dm_result *result)
{
	gpio_pin_set_dt(&dm_ranging_complete_dev, 1);
	if (!result) {
		return;
		gpio_pin_set_dt(&dm_ranging_complete_dev, 0);
	}

	if (result->quality == DM_QUALITY_CRC_FAIL) {
		LOG_INF("Quality: %d", result->quality);
		LOG_INF("Poor quality measurement, not publishing");
		gpio_pin_set_dt(&dm_ranging_complete_dev, 0);
		return;
	}

	const char *quality[DM_QUALITY_NONE + 1] = {"ok", "poor", "do not use", "crc fail", "none"};
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(&result->bt_addr, addr, sizeof(addr));

	struct dm_data dm_data = {0};
	#ifdef CONFIG_RTT_DISTANCE
	dm_data.distance = result->dist_estimates.rtt.rtt;
	#endif

	#ifdef CONFIG_MCPD_DISTANCE
	dm_data.distance = result->dist_estimates.mcpd.best;
	#endif
	memcpy(dm_data.addr, addr, sizeof(addr));

	// Get my address and convert to color:

	set_led_color(addr_to_color(addr, 17));
	#if 0
	LOG_INF("\nMeasurement result:\n");
	LOG_INF("\tAddr: %s\n", addr);
	LOG_INF("\tQuality: %s\n", quality[result->quality]);
	LOG_INF("\tDistance estimates: ");
	#ifdef CONFIG_RTT_DISTANCE
	LOG_INF("rtt: %.2f m", result->dist_estimates.rtt.rtt);
	#endif
	#ifdef CONFIG_MCPD_DISTANCE
	LOG_INF("mcpd: %.2f m", result->dist_estimates.mcpd.best);
	#endif
	#endif
	dm_data.distance -= CONFIG_DM_DISTANCE_OFFSET_CM / 100.0;
	if (dm_data.distance < 0) {
		dm_data.distance = 0;
	}
	gpio_pin_set_dt(&dm_ranging_complete_dev, 0);
	zbus_chan_pub(&dm_chan, &dm_data, K_MSEC(500));
}

static struct dm_cb dm_cb = {
	.data_ready = data_ready,
};


int main(void)
{
	gpio_pin_configure_dt(&dm_ranging_complete_dev, GPIO_OUTPUT_LOW);
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
}
