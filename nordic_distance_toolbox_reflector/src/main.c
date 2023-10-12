#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/sys/hash_function.h>

#include <zephyr/bluetooth/bluetooth.h>

#include <dm.h>

#include <advertise.h>

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

static void set_color_by_dist(int32_t dist) {


	// Get my address and convert to color:
	const char addr_local_str[BT_ADDR_LE_STR_LEN];
	bt_addr_le_t addr_local = {0};
	size_t count = 1;

	bt_id_get(&addr_local, &count);
	bt_addr_le_to_str(&addr_local, addr_local_str, sizeof(addr_local_str));

	uint32_t color_hash = sys_hash32_murmur3(addr_local_str, 17);
	color_hash = hash_to_color(color_hash);
	set_led_color(color_hash);
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
#ifdef CONFIG_BOARD_THINGY53_NRF5340_CPUAPP
	set_color_by_dist(0);
#endif
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
	set_led_color(0x0000FFFF);
#endif
}
