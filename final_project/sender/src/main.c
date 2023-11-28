#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/drivers/uart.h>
#include <Adafruit_BMP280.h>

Adafruit_BMP280 bmp280;

/* STEP 2 - Define stack size and scheduling priority used by each thread */
#define STACKSIZE 1024 
#define THREADA_PRIORITY 7
#define THREADB_PRIORITY 7
#define THREADC_PRIORITY 7

float temperature;

void threadA(void)
{
	printk("Hello, I am thread0\n");
	while (1) {
		temperature = bmp280.readTemperature();

		printk("Temperature: ");
  		printk(temperature);
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