// === Includes communs ===
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/display.h>
#include <zephyr/input/input.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>

#include <lvgl.h>
#include <lvgl_mem.h>
#include "ui/ui.h"
#include <stdio.h>
#include <limits.h>

// === Définition et configuration ===
#define STACK_SIZE 2048
K_THREAD_STACK_DEFINE(thread_stack_0, STACK_SIZE);
K_THREAD_STACK_DEFINE(thread_stack_1, STACK_SIZE);
K_THREAD_STACK_DEFINE(thread_stack_2, STACK_SIZE);

struct k_thread thread_0, thread_1, thread_2;
K_SEM_DEFINE(i2c_sem, 1, 1);
bool touch_handled = false;
LOG_MODULE_REGISTER(app);

// === Capteurs ===
const struct device *hts221  = DEVICE_DT_GET_ANY(st_hts221);
const struct device *lps22hh = DEVICE_DT_GET_ANY(st_lps22hh);
const struct device *lis2mdl = DEVICE_DT_GET_ANY(st_lis2mdl);
const struct device *lsm6dso = DEVICE_DT_GET_ANY(st_lsm6dso);

// === Touchscreen ===
static struct k_sem touch_sync;
static bool screen_touched = false;
static int current_screen = 1;
static const struct device *const touch_dev_main = DEVICE_DT_GET(DT_NODELABEL(tsc2007_adafruit_2_8_tft_touch_v2));

static void touch_event_callback(struct input_event *evt, void *user_data) {
	if (evt->code == INPUT_BTN_TOUCH) {
		screen_touched = evt->value;
	}
	if (evt->sync) {
		k_sem_give(&touch_sync);
	}
}

void init_touch_detection(const struct device *touch_dev) {
	if (!device_is_ready(touch_dev)) {
		printk("Touch device not ready\n");
		return;
	}
	k_sem_init(&touch_sync, 0, 1);
}
bool is_screen_touched(void) {
	k_sem_take(&touch_sync, K_FOREVER);
	return screen_touched;
}
INPUT_CALLBACK_DEFINE(touch_dev_main, touch_event_callback, NULL);

// === Fonctions de config capteurs ===
// (identiques aux deux fichiers - gardées telles quelles)
static void lis2mdl_config(const struct device *dev) {}
static void lps22hh_config(const struct device *dev) { /* voir original */ }
static void lsm6dso_config(const struct device *dev) { /* voir original */ }

// === Tâches capteurs ===
struct sensor_value temp1, hum, press;
struct sensor_value accel1[3], gyro[3], magn[3];

void hts221_task(void) {
	while (1) {
		k_sem_take(&i2c_sem, K_FOREVER);
		if (sensor_sample_fetch(hts221) == 0) {
			sensor_channel_get(hts221, SENSOR_CHAN_AMBIENT_TEMP, &temp1);
			sensor_channel_get(hts221, SENSOR_CHAN_HUMIDITY, &hum);
			update_display_hts221(sensor_value_to_double(&temp1), sensor_value_to_double(&hum));
		}
		k_sem_give(&i2c_sem);
		k_sleep(K_SECONDS(3));
	}
}

void lis2mdl_task(void) {
	while (1) {
		k_sem_take(&i2c_sem, K_FOREVER);
		if (sensor_sample_fetch(lis2mdl) == 0) {
			sensor_channel_get(lis2mdl, SENSOR_CHAN_MAGN_XYZ, magn);
			update_display_magn(sensor_value_to_double(magn));
		}
		k_sem_give(&i2c_sem);
		k_sleep(K_SECONDS(3));
	}
}

void lsm6dso_task(void) {
	while (1) {
		k_sem_take(&i2c_sem, K_FOREVER);
		if (sensor_sample_fetch(lsm6dso) == 0) {
			sensor_channel_get(lsm6dso, SENSOR_CHAN_ACCEL_XYZ, accel1);
			sensor_channel_get(lsm6dso, SENSOR_CHAN_GYRO_XYZ, gyro);
			update_display_accel(sensor_value_to_double(&accel1));
		}
		k_sem_give(&i2c_sem);
		k_sleep(K_SECONDS(3));
	}
}

// === Main ===
int main(void) {
	const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	if (!device_is_ready(display_dev)) {
		LOG_ERR("Display device not ready");
		return 0;
	}

	ui_init();
	display_blanking_off(display_dev);
#ifdef CONFIG_LV_Z_MEM_POOL_SYS_HEAP
	lvgl_print_heap_info(false);
#endif

	if (!device_is_ready(hts221) || !device_is_ready(lps22hh) ||
	    !device_is_ready(lis2mdl) || !device_is_ready(lsm6dso)) {
		printk("Sensor devices not ready.\n");
		return 0;
	}

	init_touch_detection(touch_dev_main);
	lis2mdl_config(lis2mdl);
	lps22hh_config(lps22hh);
	lsm6dso_config(lsm6dso);

	k_thread_create(&thread_0, thread_stack_0, K_THREAD_STACK_SIZEOF(thread_stack_0),
			hts221_task, NULL, NULL, NULL, 5, 0, K_NO_WAIT);
	k_thread_create(&thread_1, thread_stack_1, K_THREAD_STACK_SIZEOF(thread_stack_1),
			lis2mdl_task, NULL, NULL, NULL, 5, 0, K_NO_WAIT);
	k_thread_create(&thread_2, thread_stack_2, K_THREAD_STACK_SIZEOF(thread_stack_2),
			lsm6dso_task, NULL, NULL, NULL, 5, 0, K_NO_WAIT);



			while (1) {
				if (is_screen_touched()) {
					if (!touch_handled) {
						printk("Screen touched!\n");
			
						// Changement d'écran cyclique
						if (current_screen == 1) {
							_ui_screen_change(&ui_Screen2, LV_SCR_LOAD_ANIM_FADE_ON, 0, 0, &ui_Screen2_screen_init);
							current_screen = 2;
						} else if (current_screen == 2) {
							_ui_screen_change(&ui_Screen3, LV_SCR_LOAD_ANIM_FADE_ON, 0, 0, &ui_Screen3_screen_init);
							current_screen = 3;
						} else {
							_ui_screen_change(&ui_Screen1, LV_SCR_LOAD_ANIM_FADE_ON, 0, 0, &ui_Screen1_screen_init);
							current_screen = 1;
						}
			
						touch_handled = true; // On bloque jusqu’à relâchement
					}
				} else {
					// Le doigt est levé → on peut à nouveau gérer le prochain touch
					touch_handled = false;
				}
			
				lv_timer_handler();
				k_msleep(30); // Assez rapide mais pas trop pour éviter rebond
			}
			
}
