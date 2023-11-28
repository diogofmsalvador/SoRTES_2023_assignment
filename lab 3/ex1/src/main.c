/*
 * Copyright (c) 2017 Linaro Limited
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/drivers/uart.h>

/* STEP 2 - Define stack size and scheduling priority used by each thread */

void thread0(void)
{
	while (1) {
		  /* STEP 3 - Call printk() to display a simple string "Hello, I am thread0" */
		  /* STEP 6 - Make the thread yield */
		  /* STEP 10 - Put the thread to sleep */
		  /* Remember to comment out the line from STEP 6 */
	}
}

void thread1(void)
{
	while (1) {
		  /* STEP 3 - Call printk() to display a simple string "Hello, I am thread1" */
		  /* STEP 8 - Make the thread yield */
		  /* STEP 10 - Put the thread to sleep */
		  /* Remember to comment out the line from STEP 8 */
	}
}

/* STEP 4 - Define and initialize the two threads */


/* NOTE: main function required to enable USB communication */
int main() {
	if (usb_enable(NULL)) {
		return 0;
	}
	/* Busy wait for 5 seconds so you don't miss the initial printk statements to the terminal while it is connecting */
	/* Feel free to adjust longer or shorter */
	k_busy_wait(5000000);
}