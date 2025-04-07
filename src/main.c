/*
 * Copyright (c) 2019 STMicroelectronics
 *
 * SPDX-License-Identifier: Apache-2.0
 */

 #include <zephyr/kernel.h>
 #include <zephyr/device.h>
 #include <zephyr/drivers/sensor.h>
 #include <stdio.h>
 #include <zephyr/sys/util.h>
 #include <zephyr/kernel.h>
#include <zephyr/drivers/display.h>
#include <lvgl.h>
#include <lvgl_mem.h>
//#include <lv_demos.h>
#include "ui/ui.h"
#include <stdio.h>
//yes
#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#include <zephyr/logging/log.h>
 #ifdef CONFIG_LIS2MDL_TRIGGER
 static int lis2mdl_trig_cnt;
 
 static void lis2mdl_trigger_handler(const struct device *dev,
					 const struct sensor_trigger *trig)
 {
	 sensor_sample_fetch_chan(dev, SENSOR_CHAN_ALL);
	 lis2mdl_trig_cnt++;
 }
 #endif
 
 #ifdef CONFIG_LPS22HH_TRIGGER
 static int lps22hh_trig_cnt;
 
 static void lps22hh_trigger_handler(const struct device *dev,
					 const struct sensor_trigger *trig)
 {
	 sensor_sample_fetch_chan(dev, SENSOR_CHAN_PRESS);
	 lps22hh_trig_cnt++;
 }
 #endif
 

 
 #ifdef CONFIG_LSM6DSO_TRIGGER
 static int lsm6dso_acc_trig_cnt;
 static int lsm6dso_gyr_trig_cnt;
 
 static void lsm6dso_acc_trig_handler(const struct device *dev,
					  const struct sensor_trigger *trig)
 {
	 sensor_sample_fetch_chan(dev, SENSOR_CHAN_ACCEL_XYZ);
	 lsm6dso_acc_trig_cnt++;
 }
 
 static void lsm6dso_gyr_trig_handler(const struct device *dev,
					  const struct sensor_trigger *trig)
 {
	 sensor_sample_fetch_chan(dev, SENSOR_CHAN_GYRO_XYZ);
	 lsm6dso_gyr_trig_cnt++;
 }
 
 #endif
 
 static void lis2mdl_config(const struct device *lis2mdl)
 {
 
	 struct sensor_trigger trig;
 
	 trig.type = SENSOR_TRIG_DATA_READY;
	 trig.chan = SENSOR_CHAN_MAGN_XYZ;
 }
 
 static void lps22hh_config(const struct device *lps22hh)
 {
	 struct sensor_value odr_attr;
 
	 /* set LPS22HH sampling frequency to 100 Hz */
	 odr_attr.val1 = 100;
	 odr_attr.val2 = 0;
 
	 if (sensor_attr_set(lps22hh, SENSOR_CHAN_ALL,
				 SENSOR_ATTR_SAMPLING_FREQUENCY, &odr_attr) < 0) {
		 printk("Cannot set sampling frequency for LPS22HH\n");
		 return;
	 }
 
 #ifdef CONFIG_LPS22HH_TRIGGER
	 struct sensor_trigger trig;
 
	 trig.type = SENSOR_TRIG_DATA_READY;
	 trig.chan = SENSOR_CHAN_ALL;
	 sensor_trigger_set(lps22hh, &trig, lps22hh_trigger_handler);
 #endif
 }
 
 static void lsm6dso_config(const struct device *lsm6dso)
 {
	 struct sensor_value odr_attr, fs_attr;
 
	 /* set LSM6DSO accel sampling frequency to 208 Hz */
	 odr_attr.val1 = 208;
	 odr_attr.val2 = 0;
 
	 if (sensor_attr_set(lsm6dso, SENSOR_CHAN_ACCEL_XYZ,
				 SENSOR_ATTR_SAMPLING_FREQUENCY, &odr_attr) < 0) {
		 printk("Cannot set sampling frequency for LSM6DSO accel\n");
		 return;
	 }
 
	 sensor_g_to_ms2(16, &fs_attr);
 
	 if (sensor_attr_set(lsm6dso, SENSOR_CHAN_ACCEL_XYZ,
				 SENSOR_ATTR_FULL_SCALE, &fs_attr) < 0) {
		 printk("Cannot set fs for LSM6DSO accel\n");
		 return;
	 }
 
	 /* set LSM6DSO gyro sampling frequency to 208 Hz */
	 odr_attr.val1 = 208;
	 odr_attr.val2 = 0;
 
	 if (sensor_attr_set(lsm6dso, SENSOR_CHAN_GYRO_XYZ,
				 SENSOR_ATTR_SAMPLING_FREQUENCY, &odr_attr) < 0) {
		 printk("Cannot set sampling frequency for LSM6DSO gyro\n");
		 return;
	 }
 
	 sensor_degrees_to_rad(250, &fs_attr);
 
	 if (sensor_attr_set(lsm6dso, SENSOR_CHAN_GYRO_XYZ,
				 SENSOR_ATTR_FULL_SCALE, &fs_attr) < 0) {
		 printk("Cannot set fs for LSM6DSO gyro\n");
		 return;
	 }
 
 #ifdef CONFIG_LSM6DSO_TRIGGER
	 struct sensor_trigger trig;
 
	 trig.type = SENSOR_TRIG_DATA_READY;
	 trig.chan = SENSOR_CHAN_ACCEL_XYZ;
	 sensor_trigger_set(lsm6dso, &trig, lsm6dso_acc_trig_handler);
 
	 trig.type = SENSOR_TRIG_DATA_READY;
	 trig.chan = SENSOR_CHAN_GYRO_XYZ;
	 sensor_trigger_set(lsm6dso, &trig, lsm6dso_gyr_trig_handler);
 #endif
 }
 LOG_MODULE_REGISTER(app);
 int main(void)
 {

	const struct device *display_dev;

	display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	if (!device_is_ready(display_dev)) {
		LOG_ERR("Device not ready, aborting test");
		return 0;
	}

	/* Place your UI init function here */
	ui_init();
    
	display_blanking_off(display_dev);
#ifdef CONFIG_LV_Z_MEM_POOL_SYS_HEAP
	lvgl_print_heap_info(false);
#else
	printf("lvgl in malloc mode\n");
#endif
	        //This function has to be called periodically to update the UI
	        //Can be placed in a dedicated thread
		uint32_t sleep_ms = lv_timer_handler();
		k_msleep(MIN(sleep_ms, INT32_MAX));
	


	 struct sensor_value temp1, hum, press;
	 struct sensor_value accel1[3];
	 struct sensor_value gyro[3];
	 struct sensor_value magn[3];
	 const struct device *const hts221 = DEVICE_DT_GET_ANY(st_hts221);
	 const struct device *const lps22hh = DEVICE_DT_GET_ANY(st_lps22hh);
	 const struct device *const lis2mdl = DEVICE_DT_GET_ANY(st_lis2mdl);
	 const struct device *const lsm6dso = DEVICE_DT_GET_ANY(st_lsm6dso);
 
	 int cnt = 1;
 
	 if (!device_is_ready(hts221)) {
		 printk("%s: device not ready.\n", hts221->name);
		 return 0;
	 }
	 if (!device_is_ready(lps22hh)) {
		 printk("%s: device not ready.\n", lps22hh->name);
		 return 0;
	 }
	 if (!device_is_ready(lis2mdl)) {
		 printk("%s: device not ready.\n", lis2mdl->name);
		 return 0;
	 }
	 if (!device_is_ready(lsm6dso)) {
		 printk("%s: device not ready.\n", lsm6dso->name);
		 return 0;
	 }
 
	 lis2mdl_config(lis2mdl);
	 lps22hh_config(lps22hh);
	 lsm6dso_config(lsm6dso);
 
	 while (1) {
		 /* Get sensor samples */
 
		 if (sensor_sample_fetch(hts221) < 0) {
			 printf("HTS221 Sensor sample update error\n");
			 return 0;
		 }
 #ifndef CONFIG_LPS22HH_TRIGGER
		 if (sensor_sample_fetch(lps22hh) < 0) {
			 printf("LPS22HH Sensor sample update error\n");
			 return 0;
		 }
 #endif
 
 #ifndef CONFIG_LIS2MDL_TRIGGER
		 if (sensor_sample_fetch(lis2mdl) < 0) {
			 printf("LIS2MDL Magn Sensor sample update error\n");
			 return 0;
		 }
 #endif
 
 #ifndef CONFIG_LIS2DW12_TRIGGER
		 if (sensor_sample_fetch(lis2dw12) < 0) {
			 printf("LIS2DW12 Sensor sample update error\n");
			 return 0;
		 }
 #endif
 #ifndef CONFIG_LSM6DSO_TRIGGER
		 if (sensor_sample_fetch(lsm6dso) < 0) {
			 printf("LSM6DSO Sensor sample update error\n");
			 return 0;
		 }
 #endif
 
		 /* Get sensor data */
 
		 sensor_channel_get(hts221, SENSOR_CHAN_AMBIENT_TEMP, &temp1);
		 sensor_channel_get(hts221, SENSOR_CHAN_HUMIDITY, &hum);

		 sensor_channel_get(lps22hh, SENSOR_CHAN_PRESS, &press);
		 sensor_channel_get(lis2mdl, SENSOR_CHAN_MAGN_XYZ, magn);

		 sensor_channel_get(lsm6dso, SENSOR_CHAN_ACCEL_XYZ, accel1);
		 sensor_channel_get(lsm6dso, SENSOR_CHAN_GYRO_XYZ, gyro);
 
		 /* Display sensor data */

		 printf("X-NUCLEO-IKS01A3 sensor dashboard\n\n");
 
		 /* temperature */
		 printf("HTS221: Temperature: %.1f C\n",
				sensor_value_to_double(&temp1));
 
		 /* humidity */
		 printf("HTS221: Relative Humidity: %.1f%%\n",
				sensor_value_to_double(&hum));
 
		 /* pressure */
		 printf("LPS22HH: Pressure:%.3f kpa\n",
				sensor_value_to_double(&press));
 
		 /* lis2mdl */
		 printf("LIS2MDL: Magn (gauss): x: %.3f, y: %.3f, z: %.3f\n",
				sensor_value_to_double(&magn[0]),
				sensor_value_to_double(&magn[1]),
				sensor_value_to_double(&magn[2]));
 
		 printf("LSM6DSO: Accel (m.s-2): x: %.3f, y: %.3f, z: %.3f\n",
			 sensor_value_to_double(&accel1[0]),
			 sensor_value_to_double(&accel1[1]),
			 sensor_value_to_double(&accel1[2]));
 
		 printf("LSM6DSO: GYro (dps): x: %.3f, y: %.3f, z: %.3f\n",
			 sensor_value_to_double(&gyro[0]),
			 sensor_value_to_double(&gyro[1]),
			 sensor_value_to_double(&gyro[2]));
 
		 cnt++;
		 k_sleep(K_MSEC(2000));
	 }
 }