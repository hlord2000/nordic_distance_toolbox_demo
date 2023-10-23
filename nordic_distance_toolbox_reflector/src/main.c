#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/sys/hash_function.h>

#include <zephyr/bluetooth/bluetooth.h>

#include <dm.h>

#include <advertise.h>
#include <color.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

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

#endif

bool ranged = false;

void data_ready(struct dm_result *result)
{
	if (!result) {
		return;
	}
	ranged = true;

	const char *quality[DM_QUALITY_NONE + 1] = {"ok", "poor", "do not use", "crc fail", "none"};
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(&result->bt_addr, addr, sizeof(addr));
	// Convert bt_addr to uint64_t

	LOG_INF("Measurement result:\n");
	LOG_INF("Addr: %s\n", addr);
	LOG_INF("Quality: %s\n", quality[result->quality]);

	LOG_INF("Distance estimates: ");
	LOG_INF("mcpd: ifft=%.2f phase_slope=%.2f rssi_openspace=%.2f best=%.2f\n",
		result->dist_estimates.mcpd.ifft,
		result->dist_estimates.mcpd.phase_slope,
		result->dist_estimates.mcpd.rssi_openspace,
		result->dist_estimates.mcpd.best);
	// Convert the "best" estimate to a color between 0-255
}


static struct dm_cb dm_cb = {
	.data_ready = data_ready,
};

int main(void)
{
	int err;

	struct dm_init_param init_param;
	init_param.cb = &dm_cb;

#ifdef CONFIG_BOARD_THINGY53_NRF5340_CPUAPP
	if (!device_is_ready(red_pwm_led.dev)) {
		LOG_ERR("Red LED PWM device not ready\n");
	}

	if (!device_is_ready(green_pwm_led.dev)) {
		LOG_ERR("Green LED PWM device not ready\n");
	}

	if (!device_is_ready(blue_pwm_led.dev)) {
		LOG_ERR("Blue LED PWM device not ready\n");
	}
#endif

	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth failed to start (err %d)\n", err);
	}
	LOG_INF("Bluetooth initialized\n");

	err = advertise_init();
	if (err) {
		LOG_ERR("Advertising failed to start (err %d)\n", err);
	}
	LOG_INF("Advertising initialized\n");

	err = dm_init(&init_param);
	if (err) {
		LOG_ERR("Distance measurement failed to start (err %d)\n", err);
	}
	LOG_INF("Distance measurement initialized\n");
#ifdef CONFIG_BOARD_THINGY53_NRF5340_CPUAPP
	uint32_t color = 0x0000FFFF;
	while (!ranged) {
		// Make the LED fade up and down over time
		for (int i = 0; i < 255; i++) {
			color = (color & 0xFFFFFF00) | i;
			set_led_color(color);
			k_msleep(10);
		}
		for (int i = 255; i > 0; i--) {
			color = (color & 0xFFFFFF00) | i;
			set_led_color(color);
			k_msleep(10);
		}
	}
	set_led_color(hash_to_color());
#endif
}
