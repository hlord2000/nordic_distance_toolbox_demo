#include <math.h>
#include <stdint.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/sys/hash_function.h>

LOG_MODULE_REGISTER(color, LOG_LEVEL_DBG);

static float hue_to_rgb(float temp1, float temp2, float hue) {
	if (hue < 0) hue += 1;
	if (hue > 1) hue -= 1;
	if (hue < 1.0 / 6.0) return temp1 + (temp2 - temp1) * 6 * hue;
	if (hue < 0.5) return temp2;
	if (hue < 2.0 / 3.0) return temp1 + (temp2 - temp1) * (2.0 / 3.0 - hue) * 6;
	return temp1;
}

static uint32_t hsl_to_rgba(float h, float s, float l) {
    float r, g, b;
    float temp1, temp2;

    if (s == 0) {
        r = g = b = l;
    } else {
        temp2 = (l < 0.5) ? (l * (1 + s)) : ((l + s) - (s * l));
        temp1 = 2 * l - temp2;


        r = hue_to_rgb(temp1, temp2, h / 360.0 + 1.0 / 3.0);
        g = hue_to_rgb(temp1, temp2, h / 360.0);
        b = hue_to_rgb(temp1, temp2, h / 360.0 - 1.0 / 3.0);
    }

    uint32_t rgba = ((uint32_t)(r * 255) << 24) | ((uint32_t)(g * 255) << 16) | ((uint32_t)(b * 255) << 8) | 0xFF;
    return rgba;
}

uint32_t addr_to_color(char *addr, size_t length) {
	// Get my address and convert to color:

	uint32_t color_hash = sys_hash32_murmur3(addr, 17);

    float hue = (color_hash % 360) * 1.0;
    float saturation = CONFIG_INDICATOR_LED_SATURATION/100.0;
    float luminance = CONFIG_INDICATOR_LED_LUMINANCE/100.0;
    uint32_t rgba = hsl_to_rgba(hue, saturation, luminance);
    LOG_INF("Color: %x", rgba);
    LOG_INF("Hue: %f Saturation: %f Luminance: %f", hue, saturation, luminance);
    return rgba;
}
