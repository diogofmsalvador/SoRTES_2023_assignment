#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/sensor.h>

#include <stdio.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include <string.h>
#include <zephyr/bluetooth/conn.h>

/*
====================================================================================================
Global Veriables
====================================================================================================
*/

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

#define STACKSIZE       1024
#define THREADA_PRIORITY 4
#define THREADB_PRIORITY 4
#define THREADC_PRIORITY 4

#define TEAM_NUMBER 14 // Replace with your team number

#define MAX_MESSAGE_LEN 30

extern struct bt_conn *conn_connected = NULL;
static char custom_message[MAX_MESSAGE_LEN] = "";

struct sensor_value temp_readings[10];
int reading_index = 0;

static const struct gpio_dt_spec led_spec = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

static struct bt_uuid_128 custom_service_uuid = BT_UUID_INIT_128(
    0x00, 0x00, 0x44, 0x65, 0x66, 0x42, 0x56, 0xa4,
    0xd3, 0x12, 0x9b, 0xe8, 0x67, 0x45, 0x3e, 0x12
);

static struct bt_uuid_128 custom_char_uuid = BT_UUID_INIT_128(
    0x01, 0x00, 0x44, 0x65, 0x66, 0x42, 0x56, 0xa4,
    0xd3, 0x12, 0x9b, 0xe8, 0x67, 0x45, 0x3e, 0x12
);

K_SEM_DEFINE(threadA_sem, 0, 1);
K_SEM_DEFINE(threadB_sem, 0, 1);

/*
====================================================================================================
Bluetooth Ad and Sd Configuration
- Defines the Bluetooth Ad and Sd data
====================================================================================================
*/

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN)
};

static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

/*
====================================================================================================
READ CUSTOM MESSAGE Method
- Defines the message to be read when the custom characteristic is read
====================================================================================================
*/

static ssize_t read_custom_message(struct bt_conn *conn, 
                                   const struct bt_gatt_attr *attr, 
                                   void *buf, uint16_t len, uint16_t offset)
{
    const char *value = attr->user_data;

    uint16_t value_len = strlen(value);
    if (offset > value_len) {
        return BT_GATT_ERR(BT_ATT_ERR_INVALID_OFFSET);
    }
    
    uint16_t read_len = MIN(len, value_len - offset);
    memcpy(buf, value + offset, read_len);

    return read_len;
}

/*
====================================================================================================
GAT Configuration
- Defines the GATT attributes for the custom service
====================================================================================================
*/
static struct bt_gatt_attr attrs[] = {
    BT_GATT_PRIMARY_SERVICE(&custom_service_uuid),
    BT_GATT_CHARACTERISTIC(&custom_char_uuid.uuid, 
                           BT_GATT_CHRC_READ, 
                           BT_GATT_PERM_READ, 
                           read_custom_message, 
                           NULL, 
                           custom_message),
};

static struct bt_gatt_service my_service = BT_GATT_SERVICE(attrs);

/*
====================================================================================================
Bluetooth Connection Callbacks
====================================================================================================
*/

static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        printk("Connection failed (err %u)\n", err);
        return;
    }

    printk("Connected\n");
    conn_connected = bt_conn_ref(conn);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    printk("Disconnected (reason %u)\n", reason);
    if (conn == conn_connected) {
        bt_conn_unref(conn_connected); // Release the reference
        conn_connected = NULL; // Clear the connection reference
    }
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected
};

/*
====================================================================================================
Thread Methods for A, B and C keypoint of the assignment
- ThreadA reads the temperature sensor and prints the temperature to the console
- ThreadB calculates the average temperature and sends it to the custom characteristic every 10 seconds
- ThreadC turns on the LED if the temperature is over 30 degrees. If the temperature is under 30 degrees, the LED is turned off
====================================================================================================
*/

void threadA(void) {
    printk("threadA started\n");
    const struct device *dev;
    int ret;
    struct sensor_value temp_val;
    int message_number = 1;

    int reading_interval = 200;

    dev = DEVICE_DT_GET_ANY(nordic_nrf_temp);
    if (dev == NULL) {
        printf("Could not get device for temperature sensor\n");
        return;
    }

    if (!device_is_ready(dev)) {
        printf("Sensor device is not ready\n");
        return;
    }

    while (1) {
        if(reading_index >= reading_interval){
            printk("Entering Low Power Mode\n");
            k_sleep(K_FOREVER);
        }

        ret = sensor_sample_fetch(dev);
        if (ret) {
            printf("Failed to fetch sample from sensor: %d\n", ret);
            k_sleep(K_SECONDS(1));
            continue;
        }

        ret = sensor_channel_get(dev, SENSOR_CHAN_DIE_TEMP, &temp_val);
        if (ret) {
            printf("Failed to get sensor data: %d\n", ret);
            k_sleep(K_SECONDS(1));
            continue;
        }

        printf("Temperature: %d.%06dC\n", temp_val.val1, temp_val.val2);
        printf("T%02d%02d%02d\n", TEAM_NUMBER, message_number, temp_val.val1);
        message_number++;

        if (reading_index < reading_interval) {
            temp_readings[reading_index % 10] = temp_val;
            reading_index++;
            if (reading_index % 10 == 0)
            {
                k_sem_give(&threadA_sem);
            }
        }

        k_sleep(K_SECONDS(1));
    }
}

int calculate_average_temperature() {
    long total = 0;
    for (int i = 0; i < 10; i++) {
        total += temp_readings[i].val1;
    }
    return total / 10;
}

void threadB(void) {
    printk("threadB started\n");
    int send_count = 0;
    char message[MAX_MESSAGE_LEN];

    while (1) {
        k_sem_take(&threadA_sem, K_FOREVER);
        int average_temp = calculate_average_temperature();
        snprintf(message, sizeof(message), "T%02d%02d%02d", TEAM_NUMBER, send_count + 1, average_temp);

        strncpy(custom_message, message, MAX_MESSAGE_LEN);

        printf("Average Temperature: %dC\n", average_temp);
        printf("%s\n", message);
        send_count++;
        k_sem_give(&threadB_sem);
    }
}

void threadC(void) {
    printk("threadC started\n");

    if (!device_is_ready(led_spec.port)) {
        printk("Error: LED device is not ready\n");
        return;
    }

    int ret = gpio_pin_configure_dt(&led_spec, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        printk("Error: Failed to configure LED GPIO pin\n");
        return;
    }

    while (1) {
        k_sem_take(&threadB_sem, K_FOREVER);
        struct sensor_value last_temp = temp_readings[(reading_index - 1) % 10];
        if (last_temp.val1 > 30) {
            gpio_pin_set_dt(&led_spec, 1);
        } else {
            gpio_pin_set_dt(&led_spec, 0);
        }
    }
}

K_THREAD_DEFINE(threadA_id, STACKSIZE, threadA, NULL, NULL, NULL, THREADA_PRIORITY, 0, 0);
K_THREAD_DEFINE(threadB_id, STACKSIZE, threadB, NULL, NULL, NULL, THREADB_PRIORITY, 0, 0);
K_THREAD_DEFINE(threadC_id, STACKSIZE, threadC, NULL, NULL, NULL, THREADC_PRIORITY, 0, 0);

/*
====================================================================================================
Main Method
- Initializes the Bluetooth and USB devices
- Registers the custom service
- LED starts OFF
====================================================================================================
*/

int main() {
    if (usb_enable(NULL)) {
        printk("Failed to enable USB\n");
        return 1;
    }

    int err;

    printk("Starting BLE Communication...\n");

    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return 1;
    }

    printk("BLE Initialized\n");

    if (IS_ENABLED(CONFIG_SETTINGS)) {
        settings_load();
    }

    err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err) {
        printk("Advertising failed to start (err %d)\n", err);
        return 1;
    }

    bt_gatt_service_register(&my_service);

    gpio_pin_set_dt(&led_spec, 0);

    k_busy_wait(5000000);

    return 0;
}