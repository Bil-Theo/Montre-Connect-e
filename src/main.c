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
K_THREAD_STACK_DEFINE(thread_stack_area_5, STACK_SIZE);

struct k_thread thread_task_0, thread_task_1, thread_task_2, thread_task_3, thread_task_4, thread_task_5;
struct rtc_time now;
K_SEM_DEFINE(i2c_sem, 1, 1);
LOG_MODULE_REGISTER(app);

// ui.c ou main.c
int ss;
int mm;
int etat;
int step_count;
bool reset_podo;

bool touch_handled = false;

// === Capteurs ===
const struct device *hts221 = DEVICE_DT_GET_ANY(st_hts221);
const struct device *lps22hh = DEVICE_DT_GET_ANY(st_lps22hh);
const struct device *lis2mdl = DEVICE_DT_GET_ANY(st_lis2mdl);
//const struct device *lsm6dso = DEVICE_DT_GET_ANY(st_lsm6dso);
const struct device *lis2dw12 = DEVICE_DT_GET_ANY(st_lis2dw12);
uint32_t seconds = 0; // Compteur des secondes
uint32_t minutes = 0; // Compteur des minutes
uint32_t hours = 0;	  // Compteur des heures

void RTC_INIT()
{

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

void ma_rtc()
{
	// Récupérer l'appareil RTC
	const struct device *rtc_dev = DEVICE_DT_GET_ANY(microcrystal_rv_8263_c8);
	if (!device_is_ready(rtc_dev))
	{
		// Vérification si le périphérique RTC est prêt
		LOG_ERR("RTC device is not ready.");
		printk("RTC device is not ready.\n");
		return;
	}

	int ret = rtc_get_time(rtc_dev, &now);
	if (ret < 0)
	{
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

static void touch_event_callback(struct input_event *evt, void *user_data)
{
	// Désactivation du changement d'écran sur simple toucher
	if (evt->code == INPUT_BTN_TOUCH && evt->sync)
	{
		k_sem_give(&touch_sync); // Synchronisation pour d'autres usages si nécessaire
	}
}

void init_touch_detection(const struct device *touch_dev)
{
	if (!device_is_ready(touch_dev))
	{
		printk("Touch device not ready\n");
		return;
	}
	k_sem_init(&touch_sync, 0, 1);
}
bool is_screen_touched(void)
{
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
struct sensor_value accel[3], gyro[3], magn[3];

void lps22hh_task(void)
{
	struct sensor_value press;

	while (1)
	{
		k_sem_take(&i2c_sem, K_FOREVER);
		if (sensor_sample_fetch(lis2mdl) < 0)
		{
			printf("LIS2MDL sample error\n");
		}
		sensor_channel_get(lps22hh, SENSOR_CHAN_PRESS, &press);
		k_sem_give(&i2c_sem);
		// printf("LPS22HH: Pressure: %.3f kPa\n", sensor_value_to_double(&press));
		k_sleep(K_SECONDS(3));
	}
}

void lis2mdl_task(void)
{
	struct sensor_value press;

	while (1)
	{
		k_sem_take(&i2c_sem, K_FOREVER);
		if (sensor_sample_fetch(lis2mdl) < 0)
		{
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

/*
void lsm6dso_task(void)
{
	struct sensor_value press;

	while (1)
	{
		k_sem_take(&i2c_sem, K_FOREVER);
		if (sensor_sample_fetch(lsm6dso) < 0)
		{
			printf("LIS2MDL sample error\n");
		}
		sensor_channel_get(lsm6dso, SENSOR_CHAN_ACCEL_XYZ, accel1);
		sensor_channel_get(lsm6dso, SENSOR_CHAN_GYRO_XYZ, gyro);
		k_sem_give(&i2c_sem);
		printf("LSM6DSO: Accel (m/s²): x=%.3f y=%.3f z=%.3f\n",
			   sensor_value_to_double(&accel1[0]),
			   sensor_value_to_double(&accel1[1]),
			   sensor_value_to_double(&accel1[2]));
		printf("LSM6DSO: Gyro (dps): x=%.3f y=%.3f z=%.3f\n",
			   sensor_value_to_double(&gyro[0]),
			   sensor_value_to_double(&gyro[1]),
			   sensor_value_to_double(&gyro[2]));
		k_sleep(K_SECONDS(3));
	}
}
*/


void chrono_task()
{
	// Boucle infinie pour mettre à jour l'affichage toutes les secondes
	while (1)
	{
		// Calculer le temps écoulé en secondes

		if (etat == 1)
		{
			seconds++;

			// Gérer les minutes et les heures
			if (seconds >= 60)
			{
				seconds = 0;
				minutes++;
			}
			if (minutes >= 60)
			{
				minutes = 0;
				hours++;
			}
			if (hours >= 24)
			{
				hours = 0; // Remise à zéro des heures après 24 heures
			}
			update_display_chrono(minutes, seconds);
		}
		else if (etat == 2)
		{
			mm = minutes;
			ss = seconds;
			update_display_chrono(mm, ss);
		}
		else
		{
			minutes = 0;
			seconds = 0;
		}

		// Afficher le temps écoulé sous le format hh:mm:ss
		printk("%02d:%02d:%02d\n", hours, minutes, seconds);

		// Attendre 1 seconde avant de mettre à jour le chrono
		k_msleep(1000); // Attente de 1000 ms (1 seconde)
	}
}

void task_rtc(void)
{
	while (1)
	{
		k_sem_take(&i2c_sem, K_FOREVER);
		ma_rtc();
		k_sem_give(&i2c_sem);

		update_display_time(now.tm_hour, now.tm_min,
							now.tm_mday, now.tm_mon + 1, (now.tm_year + 1900) - 2000);
		k_sleep(K_MINUTES(1));
	}
}
void hts221_task(void)
{
	struct sensor_value press;

	while (1)
	{
		k_sem_take(&i2c_sem, K_FOREVER); // Prendre le
		if (sensor_sample_fetch(hts221) < 0)
		{
			printf("LIS2MDL sample error\n");
		}
		sensor_channel_get(hts221, SENSOR_CHAN_AMBIENT_TEMP, &temp1);
		sensor_channel_get(hts221, SENSOR_CHAN_HUMIDITY, &hum);

		k_sem_give(&i2c_sem); // Rendre le sémaphore

		/*printf("HTS221: Temp: %.1f C | Humidity: %.1f%%\n",
			sensor_value_to_double(&temp1),
			sensor_value_to_double(&hum));
*/
		update_display_hts221(sensor_value_to_double(&temp1),
							  sensor_value_to_double(&hum));

		k_sleep(K_SECONDS(3));
	}
}



void podometre_task(void) 
{
	typedef struct {
    		float x, y, z;
		} Vector3;
	float calculate_magnitude(Vector3 acc) { // Fonction pour calculer la magnitude d'un vecteur
    		return sqrtf(acc.x * acc.x + acc.y * acc.y + acc.z * acc.z);
		}
	
	#define STEP_THRESHOLD 13.0f
	#define MIN_TIME_BETWEEN_STEPS_MS 200 // Temps minimum entre deux pas (en ms)
	static float last_peak_time = -MIN_TIME_BETWEEN_STEPS_MS; // Temps du dernier pic détecté
	static float last_magnitude = 0.0f;
	static bool in_dead_zone = false;
	while (1)
	{
		k_sem_take(&i2c_sem, K_FOREVER);
		if (sensor_sample_fetch(lis2dw12) < 0)
		{
			printf("LIS2MDL sample error\n");
		}
		sensor_channel_get(lis2dw12, SENSOR_CHAN_ACCEL_XYZ, accel);
		k_sem_give(&i2c_sem);

		if (reset_podo)
		{
			step_count = 0;
			reset_podo = false;
		}
		// Lire les données de l'accéléromètre
		Vector3 acc_data;
		acc_data.x = sensor_value_to_double(&accel[0]);
		acc_data.y = sensor_value_to_double(&accel[1]);
		acc_data.z = sensor_value_to_double(&accel[2]);
		float magnitude = calculate_magnitude(acc_data);

		// Détecter un pic (magnitude dépasse le seuil et est un maximum local)
		if (!in_dead_zone && magnitude > STEP_THRESHOLD && magnitude > last_magnitude) {
			float current_time = k_uptime_get(); // Temps actuel en ms

			// Vérifier si le temps entre deux pics est suffisant
			if (current_time - last_peak_time > MIN_TIME_BETWEEN_STEPS_MS) {
				(step_count)++;
				last_peak_time = current_time;
				in_dead_zone = true;
			}
		}
		// Sortir de la zone morte si la magnitude redescend sous le seuil
		if (magnitude < STEP_THRESHOLD) {
			in_dead_zone = false;
		}
		last_magnitude = magnitude;

		/* update_display_podometre(step_count);
		*/
		/* printf("LIS2DW12: Accel (m.s-2): x: %.3f, y: %.3f, z: %.3f\n\n",
				sensor_value_to_double(&accel[0]),
				sensor_value_to_double(&accel[1]),
				sensor_value_to_double(&accel[2]));
	
			printf("LIS2DW12: Step count: %d\n", step_count); */
		k_msleep(100);
	}
}



// === Bluetooth ===

struct ble_data_packet {
    int16_t temperature;  // Température en dixièmes de degré (ex. 25.3°C -> 253)
    uint16_t humidity;    // Humidité en dixièmes de pourcentage (ex. 45.6% -> 456)
    uint32_t step_count;  // Nombre de pas
};

static uint8_t adv_data_buffer[sizeof(struct ble_data_packet)];

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_MANUFACTURER_DATA, adv_data_buffer, sizeof(adv_data_buffer)),
};

void prepare_advertising_data(double temperature, double humidity, uint32_t step_count)
{
    struct ble_data_packet data = {
        .temperature = (int16_t)(temperature * 10), // Convertir en dixièmes de degré
        .humidity = (uint16_t)(humidity * 10),      // Convertir en dixièmes de pourcentage
        .step_count = step_count,
    };

    // Copier les données dans le buffer d'advertising
    memcpy(adv_data_buffer, &data, sizeof(data));
}

int start_advertising(void)
{
    int err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        printk("Advertising failed to start (err %d)\n", err);
        return err;
    }
    printk("Advertising successfully started\n");
    return 0;
}


// Tâche BLE pour mettre à jour les données périodiquement
void ble_advert_task(void)
{
    double temperature = 0.0;
    double humidity = 0.0;

    while (1) {
        // Convertir les valeurs des capteurs
        temperature = sensor_value_to_double(&temp1);
        humidity = sensor_value_to_double(&hum);

        // Préparer les nouvelles données
        prepare_advertising_data(temperature, humidity, step_count);

        // Redémarrer l'advertising avec les nouvelles données
        bt_le_adv_stop();
        start_advertising();

        // Attendre avant la prochaine mise à jour
        k_sleep(K_SECONDS(2));
    }
}


// === Main ===
int main(void)
{

	int err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return 0;
    }
    printk("Bluetooth initialized\n");

    // Charger les paramètres Bluetooth
    settings_load();

    // Démarrer l'advertising initial
    prepare_advertising_data(0.0, 0.0, 0); // Données initiales
    start_advertising();


	const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	if (!device_is_ready(display_dev))
	{
		LOG_ERR("Display device not ready");
		return 0;
	}

	ui_init();
	display_blanking_off(display_dev);
#ifdef CONFIG_LV_Z_MEM_POOL_SYS_HEAP
	lvgl_print_heap_info(false);
#endif

	if (!device_is_ready(hts221) || !device_is_ready(lps22hh) ||
		!device_is_ready(lis2mdl) || !device_is_ready(lis2dw12))
	{
		printk("Sensor devices not ready.\n");
		return 0;
	}

	init_touch_detection(touch_dev_main);
	lis2mdl_config(lis2mdl);
	lps22hh_config(lps22hh);
	lsm6dso_config(lis2dw12);

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
					
					podometre_task,
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

	k_thread_create(&thread_task_5, thread_stack_area_5,
					K_THREAD_STACK_SIZEOF(thread_stack_area_5),
					ble_advert_task,
					NULL, NULL, NULL,
					1, 0, K_FOREVER);

	k_thread_start(&thread_task_0);
	k_thread_start(&thread_task_1);
	k_thread_start(&thread_task_2);
	k_thread_start(&thread_task_3);
	k_thread_start(&thread_task_4);
	k_thread_start(&thread_task_5);

	RTC_INIT();

	while (1)
	{
		if (is_screen_touched())
		{
			if (!touch_handled)
			{
				printk("Screen touched!\n");

				// Changement d'écran cyclique
				if (current_screen == 1)
				{
					_ui_screen_change(&ui_Screen2, LV_SCR_LOAD_ANIM_FADE_ON, 0, 0, &ui_Screen2_screen_init);
					current_screen = 2;
				}
				else if (current_screen == 2)
				{
					_ui_screen_change(&ui_Screen3, LV_SCR_LOAD_ANIM_FADE_ON, 0, 0, &ui_Screen3_screen_init);
					current_screen = 3;
				}
				else
				{
					_ui_screen_change(&ui_Screen1, LV_SCR_LOAD_ANIM_FADE_ON, 0, 0, &ui_Screen1_screen_init);
					current_screen = 1;
				}

				touch_handled = true; // On bloque jusqu’à relâchement
			}
		}
		else
		{
			// Le doigt est levé → on peut à nouveau gérer le prochain touch
			touch_handled = false;
		}

		lv_timer_handler();
		k_msleep(30); // Assez rapide mais pas trop pour éviter rebond
	}
}
