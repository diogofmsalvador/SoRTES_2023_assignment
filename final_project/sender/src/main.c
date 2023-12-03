#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/printk.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/gpio.h>

#include <stdio.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

#include <string.h>
#include <zephyr/bluetooth/conn.h>
// Bluetooth configuration variables
#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN)
};

static const struct bt_data sd[] = {
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};
// Global variable for the connection
extern struct bt_conn *conn_connected = NULL;

/* Custom Service UUID */
static struct bt_uuid_128 custom_service_uuid = BT_UUID_INIT_128(
    /* UUID: 123e4567-e89b-12d3-a456-426655440000 */
    0x00, 0x00, 0x44, 0x65, 0x66, 0x42, 0x56, 0xa4,
    0xd3, 0x12, 0x9b, 0xe8, 0x67, 0x45, 0x3e, 0x12
);

/* Custom Characteristic UUID */
static struct bt_uuid_128 custom_char_uuid = BT_UUID_INIT_128(
    /* UUID: 123e4567-e89b-12d3-a456-426655440001 */
    0x01, 0x00, 0x44, 0x65, 0x66, 0x42, 0x56, 0xa4,
    0xd3, 0x12, 0x9b, 0xe8, 0x67, 0x45, 0x3e, 0x12
);

#define MAX_MESSAGE_LEN 30
static char custom_message[MAX_MESSAGE_LEN] = "";

static ssize_t read_utf8(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                         void *buf, uint16_t len, uint16_t offset)
{
    return bt_gatt_attr_read(conn, attr, buf, len, offset, custom_message, strlen(custom_message));
}



static struct bt_gatt_attr attrs[] = {
    BT_GATT_PRIMARY_SERVICE(&custom_service_uuid),
    BT_GATT_CHARACTERISTIC(&custom_char_uuid.uuid,
                           BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,  // Added NOTIFY property
                           BT_GATT_PERM_READ, 
                           read_utf8, NULL, &custom_message), // Updated to reference custom_message
};



static struct bt_gatt_service my_service = BT_GATT_SERVICE(attrs);

// METHODS

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

#ifdef CONFIG_BT_LBS_SECURITY_ENABLED
static void security_changed(struct bt_conn *conn, bt_security_t level, enum bt_security_err err)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (!err) {
        printk("Security changed: %s level %u\n", addr, level);
    } else {
        printk("Security failed: %s level %u err %d\n", addr, level, err);
    }
}
#endif

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
#ifdef CONFIG_BT_LBS_SECURITY_ENABLED
    .security_changed = security_changed,
#endif
};

#if defined(CONFIG_BT_LBS_SECURITY_ENABLED)
// Keep the authentication callbacks as they are relevant for Bluetooth security
static struct bt_conn_auth_cb conn_auth_callbacks = {
    // Callback functions here...
};

static struct bt_conn_auth_info_cb conn_auth_info_callbacks = {
    // Callback functions here...
};
#else
static struct bt_conn_auth_cb conn_auth_callbacks;
static struct bt_conn_auth_info_cb conn_auth_info_callbacks;
#endif

#define STACKSIZE       1024
#define THREADA_PRIORITY 4
#define THREADB_PRIORITY 4
#define THREADC_PRIORITY 4

#define TEAM_NUMBER 01 // Replace with your team number

static const struct gpio_dt_spec led_spec = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

/* Define semaphore to monitor instances of available resource */
K_SEM_DEFINE(instance_monitor_sem, 10, 10);

K_SEM_DEFINE(threadB_sem, 0, 1);

volatile uint32_t available_instance_count = 10;

/* Array to store last 10 temperature readings */
struct sensor_value temp_readings[10];
int reading_index = 0;

/* Producer thread relinquishing access to instance */
void threadA(void) {
    printk("threadA started\n");
    const struct device *dev;
    int ret;
    struct sensor_value temp_val;
    int message_number = 1;

    printf("Starting temperature sensor example\n");

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
        printf("T%02d|%02d|%02d\n", TEAM_NUMBER, message_number, temp_val.val1);
        message_number++;

        // Store temperature reading
        temp_readings[reading_index % 10] = temp_val;
        reading_index++;

        k_sleep(K_SECONDS(1));
    }
}

/* Function to calculate average temperature */
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
    char message[MAX_MESSAGE_LEN]; // Local buffer to hold the message

    while (send_count < 20) {
        if (reading_index >= 10) {
            int average_temp = calculate_average_temperature();
            snprintf(message, sizeof(message), "T%02d|%02d|%02d", TEAM_NUMBER, send_count + 1, average_temp);

            // Update the global custom_message
            strncpy(custom_message, message, MAX_MESSAGE_LEN);

            // Notify connected clients if any
            if (conn_connected) {
                bt_gatt_notify(NULL, &attrs[1], custom_message, strlen(custom_message));
            }

            printf("Average Temperature: %dC\n", average_temp);
            printf("%s\n", message);
            send_count++;
            k_sem_give(&threadB_sem);  // Signal threadC
        }
        k_sleep(K_SECONDS(10));
    }

    printk("Entering low-power mode\n");
    k_sleep(K_FOREVER);
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
        k_sem_take(&threadB_sem, K_FOREVER);  // Wait for signal from threadB
        struct sensor_value last_temp = temp_readings[(reading_index - 1) % 10];
        if (last_temp.val1 > 30) {  // Temperature is over 30 degrees
            gpio_pin_set_dt(&led_spec, 1);  // Turn on the LED
        } else {
            gpio_pin_set_dt(&led_spec, 0);  // Turn off the LED
        }
    }
}


// Define and initialize threads
K_THREAD_DEFINE(threadA_id, STACKSIZE, threadA, NULL, NULL, NULL, THREADA_PRIORITY, 0, 0);
K_THREAD_DEFINE(threadB_id, STACKSIZE, threadB, NULL, NULL, NULL, THREADB_PRIORITY, 0, 0);
K_THREAD_DEFINE(threadC_id, STACKSIZE, threadC, NULL, NULL, NULL, THREADC_PRIORITY, 0, 0);

/* Main function required to enable USB communication */
int main() {


    if (usb_enable(NULL)) {
        printk("Failed to enable USB\n");
        return 1;
    }

    int err;

    printk("Starting Bluetooth Peripheral example\n");

    /*

    if (IS_ENABLED(CONFIG_BT_LBS_SECURITY_ENABLED)) {
        err = bt_conn_auth_cb_register(&conn_auth_callbacks);
        if (err) {
            printk("Failed to register authorization callbacks.\n");
            return 1;
        }

        err = bt_conn_auth_info_cb_register(&conn_auth_info_callbacks);
        if (err) {
            printk("Failed to register authorization info callbacks.\n");
            return 1;
        }
    }

    */

    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return 1;
    }

    printk("Bluetooth initialized\n");

    if (IS_ENABLED(CONFIG_SETTINGS)) {
        settings_load();
    }

    err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err) {
        printk("Advertising failed to start (err %d)\n", err);
        return 1;
    }

    printk("Initializing GATT service\n");
    bt_gatt_service_register(&my_service);

    printk("Advertising successfully started\n");

    /* Busy wait for 5 seconds so you don't miss the initial printk statements to the terminal while it is connecting */
    k_busy_wait(5000000);
    return 0;
}