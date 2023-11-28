#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/drivers/uart.h>

/* STEP 2 - Define stack size and scheduling priority used by each thread */
#define STACKSIZE 1024 
#define THREAD0_PRIORITY 7
#define THREAD1_PRIORITY 7
#define THREAD2_PRIORITY 7

void thread0(void)
{
	while (1) {
		printk("Hello, I am thread0\n");
		k_msleep(5);
	}
}

void thread1(void)
{
	while (1) {
		  printk("Hello, I am thread1\n");
		  k_msleep(5);
	}
}

void thread2(void)
{
	while (1) {
		  printk("Hello, I am thread2\n");
		  k_msleep(5);
	}
}

K_THREAD_DEFINE(thread0_id, STACKSIZE, thread0, NULL, NULL, NULL, THREAD0_PRIORITY, 0, 0);
K_THREAD_DEFINE(thread1_id, STACKSIZE, thread1, NULL, NULL, NULL, THREAD1_PRIORITY, 0, 0);
K_THREAD_DEFINE(thread2_id, STACKSIZE, thread2, NULL, NULL, NULL, THREAD2_PRIORITY, 0, 0);

/* NOTE: main function required to enable USB communication */
int main() {
	if (usb_enable(NULL)) {
		return 0;
	}
	/* Busy wait for 5 seconds so you don't miss the initial printk statements to the terminal while it is connecting */
	/* Feel free to adjust longer or shorter */
	k_busy_wait(5000000);
}