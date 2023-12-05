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

#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)


static bool device_name_found = false;

/* Custom Service UUID */
static struct bt_uuid_128 custom_service_uuid = BT_UUID_INIT_128(
    /* UUID: 123e4567-e89b-12d3-a456-426665440000 */
    0x00, 0x00, 0x44, 0x65, 0x66, 0x42, 0x56, 0xa4,
    0xd3, 0x12, 0x9b, 0xe8, 0x67, 0x45, 0x3e, 0x12
);

/* Custom Characteristic UUID */
static struct bt_uuid_128 custom_char_uuid = BT_UUID_INIT_128(
    /* UUID: 123e4567-e89b-12d3-a456-426665440001 */
    0x01, 0x00, 0x44, 0x65, 0x66, 0x42, 0x56, 0xa4,
    0xd3, 0x12, 0x9b, 0xe8, 0x67, 0x45, 0x3e, 0x12
);

static struct bt_conn * default_conn = NULL;

/* GATT SERVICE*/
static struct bt_gatt_subscribe_params subscribe_params;

static struct bt_gatt_discover_params discover_params;

/*static uint8_t read_cb(struct bt_conn *conn, uint8_t err, 
                       struct bt_gatt_read_params *params, 
                       const void *data, uint16_t length)
{
    if (err) {
        printk("Read error: %u\n", err);
    } else if (data) {
        printk("Characteristic value: %.*s\n", length, (const char *)data);
    }

    return BT_GATT_ITER_STOP;
} */
static uint8_t notify_cb(struct bt_conn *conn, struct bt_gatt_subscribe_params *params,
                         const void *data, uint16_t length) 
{
    if (!data) {
        if (params->value_handle == 0U) {
            printk("Unsubscribed\n");
            return BT_GATT_ITER_STOP;
        } else {
            printk("Initial subscription confirmation\n");
            return BT_GATT_ITER_CONTINUE;
        }
        
    }

    printk("Notification: %.*s\n", length, (const char *) data);
    return BT_GATT_ITER_CONTINUE;
}



static uint8_t characteristic_discovery_callback(struct bt_conn *conn, 
                                                 const struct bt_gatt_attr *attr,
                                                 struct bt_gatt_discover_params *params)
{
    if (!attr) {
        printk("Characteristic discovery completed\n");
        return BT_GATT_ITER_STOP;
    }

    const struct bt_gatt_chrc *gatt_chrc = attr->user_data;
    // printk("Custom Characteristic UUID: %s\n", bt_uuid_str(&custom_char_uuid.uuid));

    //  printk("Discovered Characteristic UUID: %s\n", bt_uuid_str(gatt_service->uuid));
    if (bt_uuid_cmp(gatt_chrc->uuid, &custom_char_uuid.uuid) == 0) {
        printk("Custom characteristic found, handle: %u\n", attr->handle);

        memset(&subscribe_params, 0, sizeof(subscribe_params));
        subscribe_params.notify = notify_cb;
        subscribe_params.value_handle = attr->handle; // Assuming the next handle is the value handle
        subscribe_params.ccc_handle = 0U; // Auto-discover CCC handle
        subscribe_params.value = BT_GATT_CCC_NOTIFY;

        int err = bt_gatt_subscribe(conn, &subscribe_params);
        if (err && (err != -EALREADY)) {
            printk("Subscribe failed (err %d)\n", err);
        } else {
            printk("Subscribed to notifications\n");
        }
    }

    return BT_GATT_ITER_STOP;
}




static uint8_t service_discovery_callback(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                          struct bt_gatt_discover_params *params)
{
    if (!attr) {
        printk("Service discovery completed\n");
        return BT_GATT_ITER_STOP;
    }

    const struct bt_gatt_chrc *gatt_service = attr->user_data;

   // printk("Custom Service UUID: %s\n", bt_uuid_str(&custom_service_uuid.uuid));

 //   printk("Discovered Service UUID: %s\n", bt_uuid_str(gatt_service->uuid));
    if (bt_uuid_cmp(gatt_service->uuid, &custom_service_uuid.uuid) == 0) {
        printk("Custom service found\n");
         memset(&discover_params, 0, sizeof(discover_params));
        discover_params.uuid = &custom_char_uuid.uuid; 
        discover_params.start_handle = attr->handle + 1;
        discover_params.end_handle = 0xFFFF; 
        discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
        discover_params.func = characteristic_discovery_callback;

        int err = bt_gatt_discover(conn, &discover_params);
        if (err) {
            printk("Characteristic discovery initiation failed (err %d)\n", err);
        } 
    } 

    return BT_GATT_ITER_CONTINUE;
}



static void start_service_discovery(struct bt_conn *conn)
{
    printk("Starting service discovery...\n");

    memset(&discover_params, 0, sizeof(discover_params));
    
    discover_params.uuid = &custom_service_uuid.uuid;
    discover_params.func = service_discovery_callback;
    discover_params.start_handle = 0x0001;
    discover_params.end_handle = 0xffff;
    discover_params.type = BT_GATT_DISCOVER_PRIMARY;

    int err = bt_gatt_discover(conn, &discover_params);
    if (err) {
        printk("GATT discovery failed (err %d)\n", err);
    }
}



/* BT */

static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        printk("Connection failed (err %u)\n", err);
        return;
    }

    printk("Connected\n");
    default_conn = bt_conn_ref(conn);  

    bt_le_scan_stop();

    start_service_discovery(conn);
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    printk("Disconnected (reason %u)\n", reason);
    if (default_conn) {
        bt_conn_unref(default_conn);  
        default_conn = NULL;
    }
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected
};

static void connect_to_sender(const bt_addr_le_t *addr)
{
    int err;

    err = bt_le_scan_stop();
	if ((err) && (err != -EALREADY)) {
		printk("Stop LE scan failed (err %d)", err);
	}

    err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, BT_LE_CONN_PARAM_DEFAULT, &default_conn);
    if (err) {
        printk("Create connection failed (err %d)\n", err);
    }
}


static bool discover_device_name(struct bt_data *data, void *user_data)
{
    const char *target_name = user_data;

    if (data->type == BT_DATA_NAME_COMPLETE || data->type == BT_DATA_NAME_SHORTENED) {
        char name[data->data_len + 1];
        memcpy(name, data->data, data->data_len);
        name[data->data_len] = '\0';  
       
        if (strncmp(name, target_name, data->data_len) == 0) {
            device_name_found = true;
            printk("Device name found: %s\n", name);
        }
    }

    return true; 
}


static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type, struct net_buf_simple *ad)
{
    device_name_found = false;
    bt_data_parse(ad, discover_device_name, "SoRTES_23_24_01");

    if (device_name_found) {
        printk("Sender found. Address: %s\n", bt_addr_le_str(addr));
        connect_to_sender(addr);
    }
}



static void bt_ready(int err)
{
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return;
    }

    printk("Bluetooth initialized\n");

    // Start scanning
    err = bt_le_scan_start(BT_LE_SCAN_ACTIVE, device_found);
    if (err) {
        printk("Scanning failed to start (err %d)\n", err);
        return;
    }

    printk("Scanning successfully started\n");
}


int main(void)
{
    if (usb_enable(NULL)) {
        printk("Failed to enable USB\n");
        return 1;
    }

    printk("Started\n");

    int err;

    printk("Starting Bluetooth Receiver\n");

    err = bt_enable(bt_ready);
    if (err) {
        printk("Bluetooth enable failed (err %d)\n", err);
    }

    k_busy_wait(5000000);
    return 0;
}
