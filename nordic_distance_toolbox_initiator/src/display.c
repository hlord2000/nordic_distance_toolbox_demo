#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <stdio.h>

#include <zephyr/drivers/display.h>
#include <lvgl.h>

#include <messages.h>

#include <dm.h>


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

	display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    int err = device_is_ready(display_dev);
    LOG_INF("Error: %d", err);
	if (!device_is_ready(display_dev)) {
        LOG_ERR("Device not ready, aborting test");
		return;
	}

    LV_IMG_DECLARE(logo);

    lv_obj_t *logo_img = lv_img_create(lv_scr_act());
    lv_img_set_src(logo_img, &logo);
    lv_obj_align(logo_img, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t *title_label = lv_label_create(lv_scr_act());
    lv_label_set_text(title_label, "Distance Toolbox");
    lv_obj_align(title_label, LV_ALIGN_BOTTOM_MID, 0, 0);
	lv_task_handler();

    k_msleep(5000);

    lv_obj_align(logo_img, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_img_set_zoom(logo_img, 256);

    lv_obj_t *ranging_label = lv_label_create(lv_scr_act());

    lv_label_set_text(ranging_label, "N/A");

    lv_obj_align(ranging_label, LV_ALIGN_TOP_RIGHT, 0, 0);

    lv_obj_t *distance_label = lv_label_create(lv_scr_act());
	lv_obj_align(distance_label, LV_ALIGN_CENTER, 0, 0);

	lv_task_handler();
	display_blanking_off(display_dev);

    char dist_str[64] = "n/a m";

    while (true) {
        if (data != NULL) {
            snprintf(dist_str, 64, "%.2f m", data->distance);
        }
        lv_label_set_text(distance_label, dist_str);

        if (data->ranging_method == DM_RANGING_MODE_MCPD) {
            lv_label_set_text(ranging_label, "MCPD");
        }

        if (data->ranging_method == DM_RANGING_MODE_RTT) {
            lv_label_set_text(ranging_label, "RTT");
        }

        lv_task_handler();
        k_msleep(100);
    }
}

K_THREAD_DEFINE(display_id, 8192, display, NULL, NULL, NULL, 7, 0, 0);
