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

/*
====================================================================================================
Global Veriables
====================================================================================================
*/

#define STACKSIZE       1024
#define THREADLED_PRIORITY 4

static struct bt_conn *default_conn;

static const char *expected_device_name = "SoRTES_23_24_Sender";
static bool server_device_found = false;

static struct bt_gatt_discover_params discover_params;

static const struct gpio_dt_spec led_spec = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);

#define MY_SERVICE_UUID_VAL 0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef0
#define MY_CHAR_UUID_VAL    0x12345678, 0x1234, 0x5678, 0x1234, 0x56789abcdef1

static struct bt_uuid_128 my_service_uuid = BT_UUID_INIT_128(
    /* UUID: 123e4567-e89b-12d3-a456-426655440000 */
    0x00, 0x00, 0x44, 0x65, 0x66, 0x42, 0x56, 0xa4,
    0xd3, 0x12, 0x9b, 0xe8, 0x67, 0x45, 0x3e, 0x12
);

static struct bt_uuid_128 my_char_uuid = BT_UUID_INIT_128(
    /* UUID: 123e4567-e89b-12d3-a456-426655440001 */
    0x01, 0x00, 0x44, 0x65, 0x66, 0x42, 0x56, 0xa4,
    0xd3, 0x12, 0x9b, 0xe8, 0x67, 0x45, 0x3e, 0x12
);

#define MAX_MESSAGE_LENGTH 100
char global_message[MAX_MESSAGE_LENGTH];

#define MAX_MESSAGE_LENGTH 100
char global_message_two[MAX_MESSAGE_LENGTH];

int led_state = 0;
int iteration_value = 0;

/*
====================================================================================================
* READ FUNCTION *
- Prints the message received from the server board
====================================================================================================
*/

struct shared_data {
    int tempValue;
};

struct shared_data led_data;

K_MUTEX_DEFINE(data_mutex);

K_SEM_DEFINE(led_control_sem, 0, 1);

void led_control_thread(void)
{
    int local_led_state = 0;
    while (1) {
        k_sem_take(&led_control_sem, K_FOREVER);

        k_mutex_lock(&data_mutex, K_FOREVER);
        int localTempValue = led_data.tempValue;
        k_mutex_unlock(&data_mutex);

        if (localTempValue > 30) {
            local_led_state = 1;
        } else {
            local_led_state = 0;
        }

        if(led_state != local_led_state){
            if (localTempValue > 30) {
                printk("Temperature is above 30, turning LED on.\n");
                led_state = 1;
                gpio_pin_set_dt(&led_spec, led_state);
            } else {
                printk("Temperature is 30 or below, turning LED off.\n");
                led_state = 0;
                gpio_pin_set_dt(&led_spec, led_state);
            }
        }
    }
}

K_THREAD_DEFINE(led_control_tid, STACKSIZE, led_control_thread, NULL, NULL, NULL, THREADLED_PRIORITY, 0, 0);

static uint8_t read_func(struct bt_conn *conn, uint8_t err,
                         struct bt_gatt_read_params *params, const void *data,
                         uint16_t length)
{
    const char *value = (const char *)data;

    if (err) {
        printk("Read error: %u\n", err);
        return BT_GATT_ITER_STOP;
    }

    if (!data) {
        return BT_GATT_ITER_STOP;
    }

    size_t message_length = (length < MAX_MESSAGE_LENGTH) ? length : (MAX_MESSAGE_LENGTH - 1);
    memcpy(global_message_two, data, message_length);
    global_message_two[message_length] = '\0';

    if (strcmp(global_message, global_message_two) != 0) {
        printk("Message Received: %.*s\n", length, (const char *)data);
    }

    char yy_str[3] = { ((const char *)data)[3], ((const char *)data)[4], '\0' };
    iteration_value = atoi(yy_str);

    if (length >= 2) {
        char lastTwoDigits[3] = {value[length - 2], value[length - 1], '\0'};
        int tempValue = atoi(lastTwoDigits);

        k_mutex_lock(&data_mutex, K_FOREVER);
        led_data.tempValue = tempValue;
        k_mutex_unlock(&data_mutex);

        k_sem_give(&led_control_sem);
    } else {
        printk("Data too short to extract last two digits\n");
    }

    size_t copy_length = (length < MAX_MESSAGE_LENGTH) ? length : (MAX_MESSAGE_LENGTH - 1);
    memcpy(global_message, data, copy_length);
    global_message[copy_length] = '\0';

    return BT_GATT_ITER_STOP;
}

/*
====================================================================================================
* DISCOVERY LOGIC *
- Tries to find the service with UUID: 123e4567-e89b-12d3-a456-426655440000
- If found, tries to find the characteristic with UUID: 123e4567-e89b-12d3-a456-426655440001
- If found, tries to read the characteristic
====================================================================================================
*/

static uint8_t characteristic_discover_func(struct bt_conn *conn,
                                            const struct bt_gatt_attr *attr,
                                            struct bt_gatt_discover_params *params)
{
    if (!attr) {
        printk("Characteristic discovery completed\n");
        return BT_GATT_ITER_STOP;
    }

    if (bt_uuid_cmp(attr->uuid, BT_UUID_GATT_CHRC) == 0) {
        static struct bt_gatt_read_params read_params;
        memset(&read_params, 0, sizeof(read_params));
        read_params.func = read_func;
        read_params.handle_count = 1;
        read_params.single.handle = attr->handle + 1;
        read_params.single.offset = 0;

        if (bt_gatt_read(conn, &read_params) < 0) {
            printk("Read request failed\n");
        }

        return BT_GATT_ITER_STOP;
    }

    return BT_GATT_ITER_CONTINUE;
}

static uint8_t discover_func(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                             struct bt_gatt_discover_params *params)
{
    if (!attr) {
        printk("Discovery completed\n");
        return BT_GATT_ITER_STOP;
    }

    if (bt_uuid_cmp(attr->uuid, BT_UUID_GATT_PRIMARY) == 0) {
        const struct bt_gatt_service_val *gatt_service;
        gatt_service = (const struct bt_gatt_service_val *)attr->user_data;
        struct bt_uuid *service_uuid = gatt_service->uuid;

        if (bt_uuid_cmp(service_uuid, &my_service_uuid.uuid) == 0) {
                discover_params.uuid = &my_char_uuid.uuid;
                discover_params.func = characteristic_discover_func;
                discover_params.start_handle = attr->handle + 1;
                discover_params.end_handle = 0xffff;
                discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

                if (bt_gatt_discover(conn, &discover_params)) {
                    printk("Characteristic discovery failed\n");
                }

                return BT_GATT_ITER_STOP;
        }
    }

    return BT_GATT_ITER_CONTINUE;
}

static void discover_service(struct bt_conn *conn)
{
    memset(&discover_params, 0, sizeof(discover_params));

    discover_params.uuid = &my_service_uuid.uuid;
    discover_params.func = discover_func;
    discover_params.start_handle = 0x0001;
    discover_params.end_handle = 0xffff;
    discover_params.type = BT_GATT_DISCOVER_PRIMARY;

    if (bt_gatt_discover(conn, &discover_params)) {
        printk("Service discovery failed\n");
    }
}

/*
====================================================================================================
* SCANNING LOGIC *
- Tries to find the server device with name: SoRTES_23_24_01
- If found, initiates a connection
====================================================================================================
*/

static bool ad_parse_callback(struct bt_data *data, void *user_data)
{
    const char *expected_name = user_data;

    if ((data->type == BT_DATA_NAME_COMPLETE || data->type == BT_DATA_NAME_SHORTENED) &&
        data->data_len == strlen(expected_name) &&
        strncmp(expected_name, data->data, data->data_len) == 0) {
        server_device_found = true;  // Set the flag if name matches
    }

    return true;
}

static void scan_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t adv_type, struct net_buf_simple *buf)
{
    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));

    server_device_found = false;
    bt_data_parse(buf, ad_parse_callback, (void *)expected_device_name);

    if (server_device_found) {
        int err;

        printf("Server device with expected name found. Address: %s\n", addr_str);

        bt_le_scan_stop();

        struct bt_conn_le_create_param *create_param = BT_CONN_LE_CREATE_PARAM(
            BT_CONN_LE_OPT_NONE,
            BT_GAP_SCAN_FAST_INTERVAL,
            BT_GAP_SCAN_FAST_WINDOW);

        err = bt_conn_le_create(addr, create_param, BT_LE_CONN_PARAM_DEFAULT, &default_conn);
        if (err) {
            printf("Connection failed (err %d)\n", err);
        } else {
            printf("Connection initiated\n");
        }
    }
}

static void start_scan(void)
{
    int err;

    static const struct bt_le_scan_param scan_param = {
        .type       = BT_HCI_LE_SCAN_ACTIVE,
        .options    = BT_LE_SCAN_OPT_NONE,
        .interval   = BT_GAP_SCAN_FAST_INTERVAL,
        .window     = BT_GAP_SCAN_FAST_WINDOW,
    };

    err = bt_le_scan_start(&scan_param, scan_cb);
    if (err) {
        printf("Scanning failed to start (err %d)\n", err);
        return;
    }

    printf("Scanning successfully started\n");
}

/*
====================================================================================================
Bluetooth Connection Callbacks
====================================================================================================
*/

static void connected(struct bt_conn *conn, uint8_t conn_err)
{
    if (conn_err) {
        printk("Failed to connect (err %u)\n", conn_err);
        return;
    }

    printk("Connected\n");
    default_conn = bt_conn_ref(conn);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    printk("Disconnected (reason %u)\n", reason);

    if (default_conn) {
        bt_conn_unref(default_conn);
        default_conn = NULL;
    }
}

static struct bt_conn_cb conn_callbacks = {
    .connected = connected,
    .disconnected = disconnected,
};

/*
====================================================================================================
Main Method
====================================================================================================
*/

int main()
{
    int err;

    if (usb_enable(NULL)) {
        printk("Failed to enable USB\n");
        return 1;
    }

    err = bt_enable(NULL);
    if (err) {
        printf("Bluetooth init failed (err %d)\n", err);
        return 1;
    }

    start_scan();
    bt_conn_cb_register(&conn_callbacks);

    int ret = gpio_pin_configure_dt(&led_spec, GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        printk("Error: Failed to configure LED GPIO pin\n");
        return;
    }

    gpio_pin_set_dt(&led_spec, led_state);

    k_busy_wait(5000000);

    while (1) {
        if (default_conn) {
            if (iteration_value != 20) {
                discover_service(default_conn);
                k_sleep(K_MSEC(1000));
            } else {
                printk("Final message received. Putting device to sleep.\n");
                k_sleep(K_FOREVER);
            }
        }
    }

    return 0;
}
