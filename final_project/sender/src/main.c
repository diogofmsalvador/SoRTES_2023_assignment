#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/drivers/uart.h>

#include <zephyr/drivers/sensor.h>
#include <stdio.h>

static double high_temp;
static double low_temp;

/* STEP 2 - Define stack size and scheduling priority used by each thread */
#define STACKSIZE 1024 
#define THREADA_PRIORITY 7
#define THREADB_PRIORITY 7
#define THREADC_PRIORITY 7

int read_temperature(const struct device *dev, struct sensor_value *val)
{
	int ret;

	ret = sensor_sample_fetch_chan(dev, SENSOR_CHAN_AMBIENT_TEMP);
	if (ret < 0) {
		printf("Could not fetch temperature: %d\n", ret);
		return ret;
	}

	ret = sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, val);
	if (ret < 0) {
		printf("Could not get temperature: %d\n", ret);
	}
	return ret;
}

void temp_alert_handler(const struct device *dev, const struct sensor_trigger *trig)
{
	int ret;
	struct sensor_value value;
	double temp;

	/* Read sensor value */
	ret = read_temperature(dev, &value);
	if (ret < 0) {
		printf("Reading temperature failed: %d\n", ret);
		return;
	}
	temp = sensor_value_to_double(&value);
	if (temp <= low_temp) {
		printf("Temperature below threshold: %0.1f°C\n", temp);
	} else if (temp >= high_temp) {
		printf("Temperature above threshold: %0.1f°C\n", temp);
	} else {
		printf("Error: temperature alert triggered without valid condition\n");
	}
}

void threadA(void)
{
	printk("Hello, I am thread0\n");
	
	const struct device *const dev = DEVICE_DT_GET(DT_ALIAS(ambient_temp0));
	struct sensor_value value;
	double temp;
	int ret;
	const struct sensor_trigger trig = {
		.chan = SENSOR_CHAN_AMBIENT_TEMP,
		.type = SENSOR_TRIG_THRESHOLD,
	};

	printf("Thermometer Example (%s)\n", CONFIG_ARCH);

	if (!device_is_ready(dev)) {
		printf("Device %s is not ready\n", dev->name);
	}

	printf("Temperature device is %p, name is %s\n", dev, dev->name);

	/* First, fetch a sensor sample to use for sensor thresholds */
	ret = read_temperature(dev, &value);
	if (ret != 0) {
		printf("Failed to read temperature: %d\n", ret);
	}
	temp = sensor_value_to_double(&value);

	/* Set thresholds to +0.5 and +1.5 °C from ambient */
	low_temp = temp + 0.5;
	ret = sensor_value_from_double(&value, low_temp);
	if (ret != 0) {
		printf("Failed to convert low threshold to sensor value: %d\n", ret);
	}
	ret = sensor_attr_set(dev, SENSOR_CHAN_AMBIENT_TEMP,
			      SENSOR_ATTR_LOWER_THRESH, &value);
	if (ret == 0) {
		/* This sensor supports threshold triggers */
		printf("Set temperature lower limit to %0.1f°C\n", low_temp);
	}

	high_temp = temp + 1.5;
	ret = sensor_value_from_double(&value, high_temp);
	if (ret != 0) {
		printf("Failed to convert low threshold to sensor value: %d\n", ret);
	}
	ret = sensor_attr_set(dev, SENSOR_CHAN_AMBIENT_TEMP,
			      SENSOR_ATTR_UPPER_THRESH, &value);
	if (ret == 0) {
		/* This sensor supports threshold triggers */
		printf("Set temperature upper limit to %0.1f°C\n", high_temp);
	}

	ret = sensor_trigger_set(dev, &trig, temp_alert_handler);
	if (ret == 0) {
		printf("Enabled sensor threshold triggers\n");
	}

	while (1) {
		ret = read_temperature(dev, &value);
		if (ret != 0) {
			printf("Failed to read temperature: %d\n", ret);
			break;
		}

		printf("Temperature is %0.1lf°C\n", sensor_value_to_double(&value));

		k_sleep(K_MSEC(1000));
	}
}

void threadB(void)
{
	printk("Hello, I am thread1\n");
	while (1) {
	}
}

void threadC(void)
{
	printk("Hello, I am thread2\n");
	while (1) {

	}
}

K_THREAD_DEFINE(threadA_id, STACKSIZE, threadA, NULL, NULL, NULL, THREADA_PRIORITY, 0, 0);
K_THREAD_DEFINE(threadB_id, STACKSIZE, threadB, NULL, NULL, NULL, THREADB_PRIORITY, 0, 0);
K_THREAD_DEFINE(threadC_id, STACKSIZE, threadC, NULL, NULL, NULL, THREADC_PRIORITY, 0, 0);


int main() {
	if (usb_enable(NULL)) {
		return 0;
	}

	k_busy_wait(5000000);
}