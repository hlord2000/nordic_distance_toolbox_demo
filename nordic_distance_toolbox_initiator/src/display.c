#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <stdio.h>

#include <zephyr/drivers/display.h>
#include <lvgl.h>

#include <messages.h>


LOG_MODULE_REGISTER(display, LOG_LEVEL_DBG);

ZBUS_CHAN_DEFINE(dm_chan, struct dm_data, NULL, NULL, ZBUS_OBSERVERS(dm_listener), ZBUS_MSG_INIT(0));

const struct dm_data *data = NULL;

static void dm_zbus_handler(const struct zbus_channel *chan) {
    data = zbus_chan_const_msg(chan);
}

ZBUS_LISTENER_DEFINE(dm_listener, dm_zbus_handler);

void display(void *p1, void *p2, void *p3)
{
    const struct device *display_dev;
	lv_obj_t *hello_world_label;

	display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    int err = device_is_ready(display_dev);
    LOG_INF("Error: %d", err);
	if (!device_is_ready(display_dev)) {
        LOG_ERR("Device not ready, aborting test");
		return;
	}
    hello_world_label = lv_label_create(lv_scr_act());
    lv_label_set_text(hello_world_label, "Hello world!");
	lv_obj_align(hello_world_label, LV_ALIGN_CENTER, 0, 0);

	lv_task_handler();
	display_blanking_off(display_dev);

    char dist_str[64] = "Distance: n/a cm";

    while (true) {
        if (data != NULL) {
            snprintf(dist_str, 64, "Distance: %.2f cm", data->distance);
        }
        lv_label_set_text(hello_world_label, dist_str);
        lv_task_handler();
        k_msleep(1000);
    }
}

K_THREAD_DEFINE(display_id, 8192, display, NULL, NULL, NULL, 7, 0, 0);