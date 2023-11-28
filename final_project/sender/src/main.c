#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/drivers/uart.h>

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>

/* STEP 2 - Define stack size and scheduling priority used by each thread */
#define STACKSIZE 1024 
#define THREADA_PRIORITY 7
#define THREADB_PRIORITY 7
#define THREADC_PRIORITY 7

LOG_MODULE_REGISTER(Info, LOG_LEVEL_INF);

static const struct device *temp_dev = DEVICE_DT_GET_ANY(nordic_nrf_temp);

/*** Time measure functions ***/
void Duration_Timer_Init()
{
	NRF_TIMER4->TASKS_START = 1;
	NRF_TIMER4->PRESCALER = 1;
}

void Duration_Timer_Start()
{
	NRF_TIMER4->TASKS_CLEAR = 1;
	NRF_TIMER4->TASKS_CAPTURE[0] = 1;
}

void Duration_Timer_Stop()
{
	NRF_TIMER4->TASKS_CAPTURE[1] = 1;
	uint32_t start = NRF_TIMER4->CC[0];
	uint32_t stop = NRF_TIMER4->CC[1];
	LOG_INF("Function duration [us]: %d", (stop - start) >> 3);	
}

/*** Performs a temperature measurement of the MCU and returns its value in degrees C ***/
static int16_t Temperature_Sensor_Get_Data()
{
	struct sensor_value temp_value;
	int err;

	Duration_Timer_Start();
    err = sensor_sample_fetch_chan(temp_dev, SENSOR_CHAN_AMBIENT_TEMP);
	Duration_Timer_Stop();

    if(err) 
        LOG_ERR("sensor_sample_fetch failed with error: %d", err);

    err = sensor_channel_get(temp_dev, SENSOR_CHAN_AMBIENT_TEMP, &temp_value);
    if(err) 
        LOG_ERR("sensor_channel_get failed with error: %d", err);

    return temp_value.val1;
}

void threadA(void)
{
	printk("Hello, I am thread0\n");
	Duration_Timer_Init();
	while (1) {
		k_msleep(1000);
		LOG_INF("MCU temperature [C]: %d", Temperature_Sensor_Get_Data());
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