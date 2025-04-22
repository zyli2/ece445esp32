#include "common.h"
#include "gap.h"
#include "gatt_svc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_err.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#define TAGAP "app_main"

void ble_store_config_init(void);

static void on_stack_reset(int reason) {
    ESP_LOGI(TAGAP, "BLE stack reset, reason: %d", reason);
}

static void on_stack_sync(void) {
    ESP_LOGI(TAGAP, "BLE stack sync complete, starting advertising...");
    adv_init();
}

static void nimble_host_config_init(void) {
    ble_hs_cfg.reset_cb = on_stack_reset;
    ble_hs_cfg.sync_cb = on_stack_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_store_config_init();
}

static void nimble_host_task(void *param) {
    ESP_LOGI(TAGAP, "Starting NimBLE host task");
    nimble_port_run();
    vTaskDelete(NULL);
}

void app_main(void) {
    esp_err_t ret;
    int rc;
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);
    // ret = nimble_port_init();
    // ESP_ERROR_CHECK(ret);
    rc = gap_init();
    if (rc != 0) {
        ESP_LOGE(TAGAP, "Failed to init GAP: %d", rc);
        return;
    }

    rc = gatt_svc_init();
    if (rc != 0) {
        ESP_LOGE(TAGAP, "Failed to init GATT: %d", rc);
        return;
    }

    nimble_host_config_init();
    xTaskCreate(nimble_host_task, "nimble_host", 20000, NULL, 5, NULL);

    ESP_LOGI(TAGAP, "BLE system initialized. Awaiting connections...");
}
