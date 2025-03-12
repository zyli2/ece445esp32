/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Includes */
#include "common.h"
#include "gap.h"
#include "led.h"
#include "motor.h"

/* Library function declarations */
void ble_store_config_init(void);

/* Private function declarations */
static void on_stack_reset(int reason);
static void on_stack_sync(void);
static void nimble_host_config_init(void);
static void nimble_host_task(void *param);

/* Private functions */
/*
 *  Stack event callback functions
 *      - on_stack_reset is called when host resets BLE stack due to errors
 *      - on_stack_sync is called when host has synced with controller
 */
static void on_stack_reset(int reason) {
    /* On reset, print reset reason to console */
    ESP_LOGI(TAG, "nimble stack reset, reset reason: %d", reason);
}

static void on_stack_sync(void) {
    /* On stack sync, do advertising initialization */
    adv_init();
}

static void nimble_host_config_init(void) {
    /* Set host callbacks */
    ble_hs_cfg.reset_cb = on_stack_reset;
    ble_hs_cfg.sync_cb = on_stack_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /* Store host configuration */
    ble_store_config_init();
}

static void nimble_host_task(void *param) {
    /* Task entry log */
    ESP_LOGI(TAG, "nimble host task has been started!");

    /* This function won't return until nimble_port_stop() is executed */
    nimble_port_run();

    /* Clean up at exit */
    vTaskDelete(NULL);
}

void app_main(void) {
    /* Local variables */
    int rc = 0;
    esp_err_t ret = ESP_OK;

    /* LED initialization */
    led_init();

    /* NVS flash initialization */
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize nvs flash, error code: %d ", ret);
        return;
    }

    /* NimBLE stack initialization */
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize nimble stack, error code: %d ",
                 ret);
        return;
    }

    /* GAP service initialization */
    rc = gap_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to initialize GAP service, error code: %d", rc);
        return;
    }

    /* NimBLE host configuration initialization */
    nimble_host_config_init();

    /* Start NimBLE host task thread and return */
    xTaskCreate(nimble_host_task, "NimBLE Host", 4*1024, NULL, 5, NULL);
    return;
}

// #include "common.h"
// #include "gap.h"
// #include "nvs_flash.h"
// #include "esp_log.h"
// #include "esp_system.h"
// #include "nimble/nimble_port.h"
// #include "nimble/nimble_port_freertos.h"
// #include "host/ble_hs.h"
// #include "host/util/util.h"
// #include "services/gatt/ble_svc_gatt.h"
// #include "services/gap/ble_svc_gap.h"
// #include "driver/gpio.h"
// // #include "host/store/ble_store_config.h"
// #include "esp_mac.h"
// #define LED_GPIO_PIN  CONFIG_BLINK_GPIO
// #define LED_SERVICE_UUID  0xABF0
// #define LED_CHAR_UUID     0xABF1

// static int led_write_cb(uint16_t conn_handle, uint16_t attr_handle,
//                         struct ble_gatt_access_ctxt *ctxt, void *arg)
// {
//     if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
//         uint16_t length = OS_MBUF_PKTLEN(ctxt->om);
//         if (length == 0) {
//             return 0;
//         }
//         uint8_t buf[32];
//         if (length > sizeof(buf) - 1) {
//             length = sizeof(buf) - 1;
//         }
//         os_mbuf_copydata(ctxt->om, 0, length, buf);
//         buf[length] = '\0';
//         if (strcasecmp((char*)buf, "on") == 0) {
//             gpio_set_level(LED_GPIO_PIN, 1);
//             ESP_LOGI(TAG, "LED turned ON");
//         } else if (strcasecmp((char*)buf, "off") == 0) {
//             gpio_set_level(LED_GPIO_PIN, 0);
//             ESP_LOGI(TAG, "LED turned OFF");
//         } else {
//             ESP_LOGW(TAG, "Unknown command: %s", buf);
//         }
//         return 0;
//     }
//     return 0;
// }

// static const struct ble_gatt_svc_def s_led_svc[] = {
//     {
//         .type = BLE_GATT_SVC_TYPE_PRIMARY,
//         .uuid = BLE_UUID16_DECLARE(LED_SERVICE_UUID),
//         .characteristics = (struct ble_gatt_chr_def[]) {
//             {
//                 .uuid = BLE_UUID16_DECLARE(LED_CHAR_UUID),
//                 .access_cb = led_write_cb,
//                 .flags = BLE_GATT_CHR_F_WRITE,
//             },
//             { 0 }
//         },
//     },
//     { 0 }
// };

// static void on_stack_reset(int reason) {
//     ESP_LOGI(TAG, "NimBLE stack reset, reason: %d", reason);
// }

// static void on_stack_sync(void) {
//     adv_init();
//     ble_svc_gatt_init();
//     ble_gatts_count_cfg(s_led_svc);
//     ble_gatts_add_svcs(s_led_svc);
//     ESP_LOGI(TAG, "Custom LED service added, now advertising.");
// }

// void ble_store_config_init(void);

// static void nimble_host_config_init(void) {
//     ble_hs_cfg.reset_cb = on_stack_reset;
//     ble_hs_cfg.sync_cb  = on_stack_sync;
//     ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
//     ble_store_config_init();
// }

// static void nimble_host_task(void *param) {
//     ESP_LOGI(TAG, "NimBLE host task started.");
//     nimble_port_run();
//     vTaskDelete(NULL);
// }

// void app_main(void) {
//     esp_err_t ret = nvs_flash_init();
//     if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
//         ESP_ERROR_CHECK(nvs_flash_erase());
//         ret = nvs_flash_init();
//     }
//     if (ret != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to initialize NVS: %d", ret);
//         return;
//     }
//     gpio_reset_pin(LED_GPIO_PIN);
//     gpio_set_direction(LED_GPIO_PIN, GPIO_MODE_OUTPUT);
//     gpio_set_level(LED_GPIO_PIN, 0);
//     ret = nimble_port_init();
//     if (ret != ESP_OK) {
//         ESP_LOGE(TAG, "Failed to init NimBLE stack: %d", ret);
//         return;
//     }
//     int rc = gap_init();
//     if (rc != 0) {
//         ESP_LOGE(TAG, "Failed to init GAP, rc=%d", rc);
//         return;
//     }
//     nimble_host_config_init();
//     xTaskCreate(nimble_host_task, "NimBLE Host", 4096, NULL, 5, NULL);
// }