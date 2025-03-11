#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gatts.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#define LED_GPIO_PIN  CONFIG_BLINK_GPIO
static const char *TAG = "NIMBLE_LED";
#define LED_SVC_UUID   0xABF0
#define LED_CHAR_UUID  0xABF1

static bool s_led_state = false;

static void configure_led(void) {
    gpio_reset_pin(LED_GPIO_PIN);
    gpio_set_direction(LED_GPIO_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO_PIN, 0);
}

static int led_chr_write_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint16_t length = OS_MBUF_PKTLEN(ctxt->om);
        uint8_t data[32];
        if (length > sizeof(data) - 1) {
            length = sizeof(data) - 1;
        }
        os_mbuf_copydata(ctxt->om, 0, length, data);
        data[length] = '\0';
        if (strcasecmp((char*)data, "on") == 0) {
            s_led_state = true;
            gpio_set_level(LED_GPIO_PIN, 1);
            ESP_LOGI(TAG, "LED turned ON (NimBLE)");
        } else if (strcasecmp((char*)data, "off") == 0) {
            s_led_state = false;
            gpio_set_level(LED_GPIO_PIN, 0);
            ESP_LOGI(TAG, "LED turned OFF (NimBLE)");
        } else {
            ESP_LOGW(TAG, "Unknown command: %s", data);
        }
        return 0;
    }
    return 0;
}

static const struct ble_gatt_svc_def gatt_services[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(LED_SVC_UUID),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = BLE_UUID16_DECLARE(LED_CHAR_UUID),
                .access_cb = led_chr_write_cb,
                .val_handle = NULL,
                .flags = BLE_GATT_CHR_F_WRITE,
            },
            { 0 }
        },
    },
    { 0 }
};

static int ble_gap_event_cb(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status != 0) {}
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        break;
    default:
        break;
    }
    return 0;
}

static void ble_app_advertise(void) {
    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UNDIRECTED;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event_cb, NULL);
}

static void ble_app_on_sync(void) {
    ble_svc_gap_device_name_set("NimBLE-LED");
    ble_app_advertise();
}

static void ble_host_task(void *param) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void init_nimble(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    esp_nimble_hci_and_controller_init();
    nimble_port_init();
    ble_hs_cfg.sync_cb = ble_app_on_sync;
    ble_hs_cfg.gatts_register_cb = NULL;
    ble_hs_cfg.store_status_cb = NULL;
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_gatts_count_cfg(gatt_services);
    ble_gatts_add_svcs(gatt_services);
    nimble_port_freertos_init(ble_host_task);
}

void app_main(void) {
    configure_led();
    init_nimble();
    ESP_LOGI(TAG, "NimBLE LED example running. Connect and write 'on'/'off' to 0xABF1.");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}