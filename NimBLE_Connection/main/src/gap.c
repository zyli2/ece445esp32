// /*
//  * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
//  *
//  * SPDX-License-Identifier: Unlicense OR CC0-1.0
//  */
// /* Includes */
// #include "gap.h"
// #include "common.h"
// #include "led.h"
// #include "motor.h"

// /* Private function declarations */
// inline static void format_addr(char *addr_str, uint8_t addr[]);
// static void print_conn_desc(struct ble_gap_conn_desc *desc);
// static void start_advertising(void);
// static int gap_event_handler(struct ble_gap_event *event, void *arg);

// /* Private variables */
// static uint8_t own_addr_type;
// static uint8_t addr_val[6] = {0};
// static uint8_t esp_uri[] = {BLE_GAP_URI_PREFIX_HTTPS, '/', '/', 'e', 's', 'p', 'r', 'e', 's', 's', 'i', 'f', '.', 'c', 'o', 'm'};

// /* Private functions */
// inline static void format_addr(char *addr_str, uint8_t addr[]) {
//     sprintf(addr_str, "%02X:%02X:%02X:%02X:%02X:%02X", addr[0], addr[1],
//             addr[2], addr[3], addr[4], addr[5]);
// }

// static void print_conn_desc(struct ble_gap_conn_desc *desc) {
//     /* Local variables */
//     char addr_str[18] = {0};

//     /* Connection handle */
//     ESP_LOGI(TAG, "connection handle: %d", desc->conn_handle);

//     /* Local ID address */
//     format_addr(addr_str, desc->our_id_addr.val);
//     ESP_LOGI(TAG, "device id address: type=%d, value=%s",
//              desc->our_id_addr.type, addr_str);

//     /* Peer ID address */
//     format_addr(addr_str, desc->peer_id_addr.val);
//     ESP_LOGI(TAG, "peer id address: type=%d, value=%s", desc->peer_id_addr.type,
//              addr_str);

//     /* Connection info */
//     ESP_LOGI(TAG,
//              "conn_itvl=%d, conn_latency=%d, supervision_timeout=%d, "
//              "encrypted=%d, authenticated=%d, bonded=%d\n",
//              desc->conn_itvl, desc->conn_latency, desc->supervision_timeout,
//              desc->sec_state.encrypted, desc->sec_state.authenticated,
//              desc->sec_state.bonded);
// }

// static void start_advertising(void) {
//     /* Local variables */
//     int rc = 0;
//     const char *name;
//     struct ble_hs_adv_fields adv_fields = {0};
//     struct ble_hs_adv_fields rsp_fields = {0};
//     struct ble_gap_adv_params adv_params = {0};

//     /* Set advertising flags */
//     adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

//     /* Set device name */
//     name = ble_svc_gap_device_name();
//     adv_fields.name = (uint8_t *)name;
//     adv_fields.name_len = strlen(name);
//     adv_fields.name_is_complete = 1;

//     /* Set device tx power */
//     adv_fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
//     adv_fields.tx_pwr_lvl_is_present = 1;

//     /* Set device appearance */
//     adv_fields.appearance = BLE_GAP_APPEARANCE_GENERIC_TAG;
//     adv_fields.appearance_is_present = 1;

//     /* Set device LE role */
//     adv_fields.le_role = BLE_GAP_LE_ROLE_PERIPHERAL;
//     adv_fields.le_role_is_present = 1;

//     /* Set advertiement fields */
//     rc = ble_gap_adv_set_fields(&adv_fields);
//     if (rc != 0) {
//         ESP_LOGE(TAG, "failed to set advertising data, error code: %d", rc);
//         return;
//     }

//     /* Set device address */
//     rsp_fields.device_addr = addr_val;
//     rsp_fields.device_addr_type = own_addr_type;
//     rsp_fields.device_addr_is_present = 1;

//     /* Set URI */
//     rsp_fields.uri = esp_uri;
//     rsp_fields.uri_len = sizeof(esp_uri);

//     /* Set advertising interval */
//     rsp_fields.adv_itvl = BLE_GAP_ADV_ITVL_MS(500);
//     rsp_fields.adv_itvl_is_present = 1;

//     /* Set scan response fields */
//     rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
//     if (rc != 0) {
//         ESP_LOGE(TAG, "failed to set scan response data, error code: %d", rc);
//         return;
//     }

//     /* Set non-connetable and general discoverable mode to be a beacon */
//     adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
//     adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

//     /* Set advertising interval */
//     adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(500);
//     adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(510);

//     /* Start advertising */
//     rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params,
//                            gap_event_handler, NULL);
//     if (rc != 0) {
//         ESP_LOGE(TAG, "failed to start advertising, error code: %d", rc);
//         return;
//     }
//     ESP_LOGI(TAG, "advertising started!");
// }

// /*
//  * NimBLE applies an event-driven model to keep GAP service going
//  * gap_event_handler is a callback function registered when calling
//  * ble_gap_adv_start API and called when a GAP event arrives
//  */
// static int gap_event_handler(struct ble_gap_event *event, void *arg) {
//     /* Local variables */
//     int rc = 0;
//     struct ble_gap_conn_desc desc;

//     /* Handle different GAP event */
//     switch (event->type) {

//     /* Connect event */
//     case BLE_GAP_EVENT_CONNECT:
//         /* A new connection was established or a connection attempt failed. */
//         ESP_LOGI(TAG, "connection %s; status=%d",
//                  event->connect.status == 0 ? "established" : "failed",
//                  event->connect.status);

//         /* Connection succeeded */
//         if (event->connect.status == 0) {
//             /* Check connection handle */
//             rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
//             if (rc != 0) {
//                 ESP_LOGE(TAG,
//                          "failed to find connection by handle, error code: %d",
//                          rc);
//                 return rc;
//             }

//             /* Print connection descriptor and turn on the LED */
//             print_conn_desc(&desc);
//             led_on();
//             cmd_gpio_on();

//             /* Try to update connection parameters */
//             struct ble_gap_upd_params params = {.itvl_min = desc.conn_itvl,
//                                                 .itvl_max = desc.conn_itvl,
//                                                 .latency = 3,
//                                                 .supervision_timeout =
//                                                     desc.supervision_timeout};
//             rc = ble_gap_update_params(event->connect.conn_handle, &params);
//             if (rc != 0) {
//                 ESP_LOGE(
//                     TAG,
//                     "failed to update connection parameters, error code: %d",
//                     rc);
//                 return rc;
//             }
//         }
//         /* Connection failed, restart advertising */
//         else {
//             start_advertising();
//         }
//         return rc;

//     /* Disconnect event */
//     case BLE_GAP_EVENT_DISCONNECT:
//         /* A connection was terminated, print connection descriptor */
//         ESP_LOGI(TAG, "disconnected from peer; reason=%d",
//                  event->disconnect.reason);

//         /* Turn off the LED */
//         led_off();
//         cmd_gpio_off();

//         /* Restart advertising */
//         start_advertising();
//         return rc;

//     /* Connection parameters update event */
//     case BLE_GAP_EVENT_CONN_UPDATE:
//         /* The central has updated the connection parameters. */
//         ESP_LOGI(TAG, "connection updated; status=%d",
//                  event->conn_update.status);

//         /* Print connection descriptor */
//         rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
//         if (rc != 0) {
//             ESP_LOGE(TAG, "failed to find connection by handle, error code: %d",
//                      rc);
//             return rc;
//         }
//         print_conn_desc(&desc);
//         return rc;
//     }

//     return rc;
// }

// /* Public functions */
// void adv_init(void) {
//     /* Local variables */
//     int rc = 0;
//     char addr_str[18] = {0};

//     /* Make sure we have proper BT identity address set */
//     rc = ble_hs_util_ensure_addr(0);
//     if (rc != 0) {
//         ESP_LOGE(TAG, "device does not have any available bt address!");
//         return;
//     }

//     /* Figure out BT address to use while advertising */
//     rc = ble_hs_id_infer_auto(0, &own_addr_type);
//     if (rc != 0) {
//         ESP_LOGE(TAG, "failed to infer address type, error code: %d", rc);
//         return;
//     }

//     /* Copy device address to addr_val */
//     rc = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);
//     if (rc != 0) {
//         ESP_LOGE(TAG, "failed to copy device address, error code: %d", rc);
//         return;
//     }
//     format_addr(addr_str, addr_val);
//     ESP_LOGI(TAG, "device address: %s", addr_str);

//     /* Start advertising. */
//     start_advertising();
// }

// int gap_init(void) {
//     /* Local variables */
//     int rc = 0;

//     /* Initialize GAP service */
//     ble_svc_gap_init();

//     /* Set GAP device name */
//     rc = ble_svc_gap_device_name_set(DEVICE_NAME);
//     if (rc != 0) {
//         ESP_LOGE(TAG, "failed to set device name to %s, error code: %d",
//                  DEVICE_NAME, rc);
//         return rc;
//     }

//     /* Set GAP device appearance */
//     rc = ble_svc_gap_device_appearance_set(BLE_GAP_APPEARANCE_GENERIC_TAG);
//     if (rc != 0) {
//         ESP_LOGE(TAG, "failed to set device appearance, error code: %d", rc);
//         return rc;
//     }
//     return rc;
// }

#include "gap.h"
#include "common.h"
#include "led.h"
#include "motor.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

/* Change this if your logs need a different tag */
// static const char *TAG = "GAP_EXAMPLE";

/* Example UUIDs for custom service/characteristic */
#define LED_SERVICE_UUID  0xABF0
#define LED_CHAR_UUID     0xABF1

/* For NimBLE address handling */
static uint8_t own_addr_type;
static uint8_t addr_val[6] = {0};

/* Example URI for scan response */
static uint8_t esp_uri[] = {
    BLE_GAP_URI_PREFIX_HTTPS, '/', '/', 'e', 's', 'p', 'r', 'e', 's', 's', 'i', 'f', '.', 'c', 'o', 'm'
};

/* Forward declarations */
static int gap_event_handler(struct ble_gap_event *event, void *arg);
static void start_advertising(void);

/* Writes to the LED Characteristic come here */
static int led_write_cb(uint16_t conn_handle,
                        uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt,
                        void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint16_t length = OS_MBUF_PKTLEN(ctxt->om);
        if (length == 0) {
            return 0;
        }
        uint8_t buf[32];
        if (length > sizeof(buf) - 1) {
            length = sizeof(buf) - 1;
        }
        os_mbuf_copydata(ctxt->om, 0, length, buf);
        buf[length] = '\0';
        /* Compare with "on" or "off" */
        if (strcasecmp((char*)buf, "on") == 0) {
            led_on();
            ESP_LOGI(TAG, "LED turned ON via BLE write");
        } else if (strcasecmp((char*)buf, "off") == 0) {
            led_off();
            ESP_LOGI(TAG, "LED turned OFF via BLE write");
        } else {
            ESP_LOGW(TAG, "Unknown command: %s", buf);
        }
    }
    return 0;
}

/* Define a custom service with a single writable characteristic. */
static const struct ble_gatt_svc_def s_led_svc[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(LED_SERVICE_UUID),
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = BLE_UUID16_DECLARE(LED_CHAR_UUID),
                .access_cb = led_write_cb,
                .flags = BLE_GATT_CHR_F_WRITE,
            },
            {0}
        },
    },
    {0}
};

static void format_addr(char *addr_str, uint8_t addr[]) {
    sprintf(addr_str, "%02X:%02X:%02X:%02X:%02X:%02X",
            addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

/* Print connection details when connected */
static void print_conn_desc(struct ble_gap_conn_desc *desc) {
    char addr_str[18] = {0};
    ESP_LOGI(TAG, "connection handle: %d", desc->conn_handle);
    format_addr(addr_str, desc->our_id_addr.val);
    ESP_LOGI(TAG, "device id address: type=%d, value=%s",
             desc->our_id_addr.type, addr_str);
    format_addr(addr_str, desc->peer_id_addr.val);
    ESP_LOGI(TAG, "peer id address: type=%d, value=%s",
             desc->peer_id_addr.type, addr_str);
    ESP_LOGI(TAG,
             "conn_itvl=%d, conn_latency=%d, supervision_timeout=%d, "
             "encrypted=%d, authenticated=%d, bonded=%d",
             desc->conn_itvl, desc->conn_latency, desc->supervision_timeout,
             desc->sec_state.encrypted, desc->sec_state.authenticated,
             desc->sec_state.bonded);
}

/* GAP event handler */
static int gap_event_handler(struct ble_gap_event *event, void *arg) {
    int rc;
    struct ble_gap_conn_desc desc;

    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "connection %s; status=%d",
                 event->connect.status == 0 ? "established" : "failed",
                 event->connect.status);
        if (event->connect.status == 0) {
            rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
            if (rc == 0) {
                print_conn_desc(&desc);
                struct ble_gap_upd_params params = {
                    .itvl_min = desc.conn_itvl,
                    .itvl_max = desc.conn_itvl,
                    .latency = 3,
                    .supervision_timeout = desc.supervision_timeout
                };
                rc = ble_gap_update_params(event->connect.conn_handle, &params);
                if (rc != 0) {
                    ESP_LOGE(TAG, "failed updating conn params, rc=%d", rc);
                }
            } else {
                ESP_LOGE(TAG, "conn_find failed, rc=%d", rc);
            }
        } else {
            start_advertising();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "disconnected from peer; reason=%d",
                 event->disconnect.reason);
        start_advertising();
        break;

    case BLE_GAP_EVENT_CONN_UPDATE:
        ESP_LOGI(TAG, "connection updated; status=%d",
                 event->conn_update.status);
        rc = ble_gap_conn_find(event->conn_update.conn_handle, &desc);
        if (rc == 0) {
            print_conn_desc(&desc);
        }
        break;

    default:
        break;
    }
    return 0;
}

static void start_advertising(void) {
    int rc;
    const char *name;
    struct ble_hs_adv_fields adv_fields = {0};
    struct ble_hs_adv_fields rsp_fields = {0};
    struct ble_gap_adv_params adv_params = {0};

    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    name = ble_svc_gap_device_name();
    adv_fields.name = (uint8_t *)name;
    adv_fields.name_len = strlen(name);
    adv_fields.name_is_complete = 1;
    adv_fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
    adv_fields.tx_pwr_lvl_is_present = 1;
    adv_fields.appearance = BLE_GAP_APPEARANCE_GENERIC_TAG;
    adv_fields.appearance_is_present = 1;
    adv_fields.le_role = BLE_GAP_LE_ROLE_PERIPHERAL;
    adv_fields.le_role_is_present = 1;

    rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set adv data, rc=%d", rc);
        return;
    }

    rsp_fields.device_addr = addr_val;
    rsp_fields.device_addr_type = own_addr_type;
    rsp_fields.device_addr_is_present = 1;
    rsp_fields.uri = esp_uri;
    rsp_fields.uri_len = sizeof(esp_uri);
    rsp_fields.adv_itvl = BLE_GAP_ADV_ITVL_MS(500);
    rsp_fields.adv_itvl_is_present = 1;

    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set scan rsp data, rc=%d", rc);
        return;
    }

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(500);
    adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(510);

    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &adv_params,
                           gap_event_handler, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv start failed, rc=%d", rc);
        return;
    }
    ESP_LOGI(TAG, "advertising started!");
}

/* Register custom service and start adv */
void adv_init(void) {
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "no available bt address, rc=%d", rc);
        return;
    }
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to infer addr type, rc=%d", rc);
        return;
    }
    rc = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed copying dev addr, rc=%d", rc);
        return;
    }
    char addr_str[18] = {0};
    format_addr(addr_str, addr_val);
    ESP_LOGI(TAG, "device address: %s", addr_str);

    ble_svc_gatt_init();
    ble_gatts_count_cfg(s_led_svc);
    ble_gatts_add_svcs(s_led_svc);
    start_advertising();
}

int gap_init(void) {
    int rc;
    ble_svc_gap_init();
    rc = ble_svc_gap_device_name_set(DEVICE_NAME);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set device name=%s, rc=%d", DEVICE_NAME, rc);
        return rc;
    }
    rc = ble_svc_gap_device_appearance_set(BLE_GAP_APPEARANCE_GENERIC_TAG);
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to set appearance, rc=%d", rc);
        return rc;
    }
    return 0;
}