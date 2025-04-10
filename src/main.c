// === Includes communs ===
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/display.h>
#include <zephyr/input/input.h>
#include <zephyr/sys/util.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/rtc.h>
#include <lvgl.h>
#include <lvgl_mem.h>
#include "ui/ui.h"
#include <stdio.h>
#include <limits.h>

#include <zephyr/drivers/gpio.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <bluetooth/services/nus.h>

// === Définition et configuration ===
#define STACK_SIZE 2048
K_THREAD_STACK_DEFINE(thread_stack_area_0, STACK_SIZE);
K_THREAD_STACK_DEFINE(thread_stack_area_1, STACK_SIZE);
K_THREAD_STACK_DEFINE(thread_stack_area_2, STACK_SIZE);
K_THREAD_STACK_DEFINE(thread_stack_area_3, STACK_SIZE);
K_THREAD_STACK_DEFINE(thread_stack_area_4, STACK_SIZE);

struct k_thread thread_task_0, thread_task_1, thread_task_2, thread_task_3, thread_task_4;
struct rtc_time now;
K_SEM_DEFINE(i2c_sem, 1, 1);
LOG_MODULE_REGISTER(app);

// ui.c ou main.c
int ss;
int mm;
int etat;



bool touch_handled = false;

// === Capteurs ===
const struct device *hts221  = DEVICE_DT_GET_ANY(st_hts221);
const struct device *lps22hh = DEVICE_DT_GET_ANY(st_lps22hh);
const struct device *lis2mdl = DEVICE_DT_GET_ANY(st_lis2mdl);
const struct device *lsm6dso = DEVICE_DT_GET_ANY(st_lsm6dso);
uint32_t seconds = 0;  // Compteur des secondes
uint32_t minutes = 0;  // Compteur des minutes
uint32_t hours = 0;    // Compteur des heures


void RTC_INIT(){


	const struct device *rtc_dev = DEVICE_DT_GET(DT_NODELABEL(rv8263));

	device_is_ready(rtc_dev);

	struct rtc_time date = {
		.tm_sec = 0,
		.tm_min = 32,
		.tm_hour = 18,
		.tm_mday = 10,
		.tm_mon = 3,
		.tm_year = 2025 - 1900,
	};

	rtc_set_time(rtc_dev, &date);
}


void ma_rtc() {
    // Récupérer l'appareil RTC
    const struct device *rtc_dev = DEVICE_DT_GET_ANY(microcrystal_rv_8263_c8);
    if (!device_is_ready(rtc_dev)) {
        // Vérification si le périphérique RTC est prêt
        LOG_ERR("RTC device is not ready.");
        printk("RTC device is not ready.\n");
        return;
    }

    int ret = rtc_get_time(rtc_dev, &now);
    if (ret < 0) {
        // Si l'appel pour obtenir l'heure échoue
        LOG_ERR("Failed to get RTC time");
        printk("Failed to get RTC time\n");
        return;
    }

    // Afficher l'heure dans un format lisible
    /*printk("Current time: %02d:%02d:%02d %02d/%02d/%04d\n",
           now.tm_hour, now.tm_min, now.tm_sec,
           now.tm_mday, now.tm_mon + 1, now.tm_year + 1900);*/
}


// === Touchscreen ===
static struct k_sem touch_sync;
static bool screen_touched = false;
static int current_screen = 1;
static const struct device *const touch_dev_main = DEVICE_DT_GET(DT_NODELABEL(tsc2007_adafruit_2_8_tft_touch_v2));

static void bt_ready(int err)
{
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return;
    }

    printk("Bluetooth initialized\n");

    err = bt_le_adv_start(BT_LE_ADV_PARAM( BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_USE_NAME, BT_GAP_ADV_FAST_INT_MIN_2,BT_GAP_ADV_FAST_INT_MAX_2,NULL),
        NULL, 0, NULL, 0);

    if (err) {
        printk("Advertising failed to start (err %d)\n", err);
    }
}

static void touch_event_callback(struct input_event *evt, void *user_data) {
    // Désactivation du changement d'écran sur simple toucher
    if (evt->code == INPUT_BTN_TOUCH && evt->sync) {
        k_sem_give(&touch_sync); // Synchronisation pour d'autres usages si nécessaire
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

void lps22hh_task(void)
{
	struct sensor_value press;

	while (1) {
		k_sem_take(&i2c_sem, K_FOREVER);
		if (sensor_sample_fetch(lis2mdl) < 0) {
			printf("LIS2MDL sample error\n");

		}
		sensor_channel_get(lps22hh, SENSOR_CHAN_PRESS, &press);
		k_sem_give(&i2c_sem);
		//printf("LPS22HH: Pressure: %.3f kPa\n", sensor_value_to_double(&press));
		k_sleep(K_SECONDS(3));
	}
}


void lis2mdl_task(void)
{
	struct sensor_value press;

	while (1) {
		k_sem_take(&i2c_sem, K_FOREVER);
		if (sensor_sample_fetch(lis2mdl) < 0) {
			printf("LIS2MDL sample error\n");
		}
		
		sensor_channel_get(lis2mdl, SENSOR_CHAN_MAGN_XYZ, magn);
		k_sem_give(&i2c_sem);
		/*printf("LIS2MDL: Magnetometer (gauss): x=%.3f y=%.3f z=%.3f\n",
		       sensor_value_to_double(&magn[0]),
		       sensor_value_to_double(&magn[1]),
		       sensor_value_to_double(&magn[2]));*/
		k_sleep(K_SECONDS(3));
	}
}

void lsm6dso_task(void)
{
	struct sensor_value press;

	while (1) {
		k_sem_take(&i2c_sem, K_FOREVER);
		if (sensor_sample_fetch(lsm6dso) < 0) {
			printf("LIS2MDL sample error\n");
		}
		sensor_channel_get(lsm6dso, SENSOR_CHAN_ACCEL_XYZ, accel1);
		sensor_channel_get(lsm6dso, SENSOR_CHAN_GYRO_XYZ, gyro);
		k_sem_give(&i2c_sem);
		/*printf("LSM6DSO: Accel (m/s²): x=%.3f y=%.3f z=%.3f\n",
		       sensor_value_to_double(&accel1[0]),
		       sensor_value_to_double(&accel1[1]),
		       sensor_value_to_double(&accel1[2]));
		printf("LSM6DSO: Gyro (dps): x=%.3f y=%.3f z=%.3f\n",
		       sensor_value_to_double(&gyro[0]),
		       sensor_value_to_double(&gyro[1]),
		       sensor_value_to_double(&gyro[2]));
		k_sleep(K_SECONDS(3));*/
	}
}

void chrono_task() {
    // Boucle infinie pour mettre à jour l'affichage toutes les secondes
    while (1) {
        // Calculer le temps écoulé en secondes
        
		if(etat == 1){
			seconds++;

        // Gérer les minutes et les heures
        if (seconds >= 60) {
            seconds = 0;
            minutes++;
        }
        if (minutes >= 60) {
            minutes = 0;
            hours++;
        }
        if (hours >= 24) {
            hours = 0; // Remise à zéro des heures après 24 heures
        }
			update_display_chrono(minutes, seconds);
		}
		else if(etat == 2){
			mm = minutes;
			ss = seconds;
			update_display_chrono(mm, ss);
		}
		else{
			minutes = 0;
			seconds = 0;
		}

        // Afficher le temps écoulé sous le format hh:mm:ss
        printk("%02d:%02d:%02d\n", hours, minutes, seconds);

        // Attendre 1 seconde avant de mettre à jour le chrono
        k_msleep(1000);  // Attente de 1000 ms (1 seconde)
    }
}

void task_rtc(void)
{
	while (1) {
		k_sem_take(&i2c_sem, K_FOREVER);
		ma_rtc();
		k_sem_give(&i2c_sem);

		update_display_time(now.tm_hour, now.tm_min,
			now.tm_mday, now.tm_mon + 1, (now.tm_year + 1900)-2000);
		k_sleep(K_MINUTES(1));
	}
}
void hts221_task(void)
{
	struct sensor_value press;

	while (1) {
		k_sem_take(&i2c_sem, K_FOREVER);//Prendre le 
		if (sensor_sample_fetch(hts221) < 0) {
			printf("LIS2MDL sample error\n");
		}
		sensor_channel_get(hts221, SENSOR_CHAN_AMBIENT_TEMP, &temp1);
		sensor_channel_get(hts221, SENSOR_CHAN_HUMIDITY, &hum);

		k_sem_give(&i2c_sem);//Rendre le sémaphore

		/*printf("HTS221: Temp: %.1f C | Humidity: %.1f%%\n",
			sensor_value_to_double(&temp1),
			sensor_value_to_double(&hum));
*/
		update_display_hts221(sensor_value_to_double(&temp1),
				      sensor_value_to_double(&hum));

		k_sleep(K_SECONDS(3));
	}
}
// === Main ===
int main(void) {

	int err;

    err = bt_enable(bt_ready);
    if (err) {
        printk("Bluetooth initialization failed (err %d)\n", err);
        return;
    }

    printk("Bluetooth initialized successfully\n");

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

	k_thread_create(&thread_task_0, thread_stack_area_0,
		K_THREAD_STACK_SIZEOF(thread_stack_area_0),
		lis2mdl_task,
		NULL, NULL, NULL,
		5, 0, K_NO_WAIT);

	 k_thread_create(&thread_task_1, thread_stack_area_1,
		K_THREAD_STACK_SIZEOF(thread_stack_area_1),
		hts221_task,
		NULL, NULL, NULL,
		5, 0, K_NO_WAIT);

	k_thread_create(&thread_task_2, thread_stack_area_2,
		K_THREAD_STACK_SIZEOF(thread_stack_area_2),
		lsm6dso_task,
		NULL, NULL, NULL,
		1, 0, K_FOREVER);

		k_thread_create(&thread_task_3, thread_stack_area_3,
			K_THREAD_STACK_SIZEOF(thread_stack_area_3),
			task_rtc,
			NULL, NULL, NULL,
			1, 0, K_FOREVER);

			k_tid_t id5 = k_thread_create(&thread_task_4, thread_stack_area_4,
				K_THREAD_STACK_SIZEOF(thread_stack_area_4),
				chrono_task,
				NULL, NULL, NULL,
				1, 0, K_FOREVER);
			k_thread_start(&thread_task_0);
			k_thread_start(&thread_task_1);
			k_thread_start(&thread_task_2);
			k_thread_start(&thread_task_3);
			k_thread_start(&thread_task_4);
	

				RTC_INIT();

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
