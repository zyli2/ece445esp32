/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include "common.h"
#include "gap.h"
#include "gatt_svc.h"
#include "heart_rate.h"
#include "led.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

/* ───────────────────────────────── DS18B20 includes ───────────────────────── */
#include "owb.h"
#include "owb_rmt.h"
#include "ds18b20.h"

/* ─────────────────────────────── Compile-time opts ────────────────────────── */
#ifndef CONFIG_ONE_WIRE_GPIO
#define CONFIG_ONE_WIRE_GPIO 10        // change in menuconfig if desired
#endif
#define MAX_SENSORS        8
#define DS18B20_RES        DS18B20_RESOLUTION_12_BIT
#define TEMP_PERIOD_MS     1000

/* ────────────────────────────── Forward prototypes ────────────────────────── */
static void nimble_host_task(void *param);
static void heart_rate_task(void *param);
static void ds18b20_task(void *param);

void ble_store_config_init(void);

/* ─────────────────────── NimBLE stack helper callbacks ────────────────────── */
static void on_stack_reset(int reason)
{
    ESP_LOGI(TAG, "NimBLE stack reset, reason=%d", reason);
}
static void on_stack_sync(void)
{
    adv_init();         /* start advertising after host/controller sync */
}
static void nimble_host_config_init(void)
{
    ble_hs_cfg.reset_cb          = on_stack_reset;
    ble_hs_cfg.sync_cb           = on_stack_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb   = ble_store_util_status_rr;
    ble_store_config_init();
}

/* ───────────────────────────── Heart-Rate task ────────────────────────────── */
static void heart_rate_task(void *param)
{
    ESP_LOGI(TAG, "Heart-Rate task started");
    while (true) {
        update_heart_rate();
        ESP_LOGI(TAG, "Heart-Rate = %d bpm", get_heart_rate());
        send_heart_rate_indication();
        vTaskDelay(HEART_RATE_TASK_PERIOD);
    }
}

/* ───────────────────────────── DS18B20 helpers ────────────────────────────── */
typedef struct {
    OneWireBus        *bus;
    OneWireBus_ROMCode roms[MAX_SENSORS];
    DS18B20_Info      *sensors[MAX_SENSORS];
    int                count;
} TempCtx;

static OneWireBus *init_onewire(int gpio)
{
    static owb_rmt_driver_info rmt;              /* static – persists forever */
    OneWireBus *bus = owb_rmt_initialize(&rmt, gpio, RMT_CHANNEL_1, RMT_CHANNEL_0);
    owb_use_crc(bus, true);
    ESP_LOGI("OWB", "1-Wire bus on GPIO %d", gpio);
    return bus;
}

static int discover_devices(OneWireBus *bus, OneWireBus_ROMCode *out)
{
    OneWireBus_SearchState st = {0};
    bool found; int n = 0;
    owb_search_first(bus, &st, &found);
    while (found && n < MAX_SENSORS) {
        out[n++] = st.rom_code;
        owb_search_next(bus, &st, &found);
    }
    ESP_LOGI("OWB", "Discovered %d DS18B20 sensor%s", n, n==1 ? "" : "s");
    return n;
}

static void init_sensor(DS18B20_Info *info, OneWireBus *bus,
                        const OneWireBus_ROMCode *rom, bool solo)
{
    solo ? ds18b20_init_solo(info, bus)
         : ds18b20_init(info, bus, *rom);
    ds18b20_use_crc(info, true);
    ds18b20_set_resolution(info, DS18B20_RES);
}

/* ───────────────────────────── DS18B20 task ──────────────────────────────── */
static void ds18b20_task(void *param)
{
    /* tiny delay to be nice to the power-up caps on VDD */
    vTaskDelay(pdMS_TO_TICKS(2000));

    TempCtx ctx = {0};
    ctx.bus   = init_onewire(CONFIG_ONE_WIRE_GPIO);
    ctx.count = discover_devices(ctx.bus, ctx.roms);

    if (ctx.count == 0) {
        ESP_LOGW("DS18B20", "No sensors found – rebooting in 5 s");
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
    }

    for (int i = 0; i < ctx.count; ++i) {
        ctx.sensors[i] = ds18b20_malloc();
        init_sensor(ctx.sensors[i], ctx.bus, &ctx.roms[i], ctx.count == 1);
    }

    int err[MAX_SENSORS] = {0};
    int sample = 0;
    TickType_t wake = xTaskGetTickCount();

    ESP_LOGI("DS18B20", "Temperature task started");
    for (;;) {
        ds18b20_convert_all(ctx.bus);
        ds18b20_wait_for_conversion(ctx.sensors[0]);

        ESP_LOGI("TEMP", "Sample %d", ++sample);
        for (int i = 0; i < ctx.count; ++i) {
            float t;
            DS18B20_ERROR e = ds18b20_read_temp(ctx.sensors[i], &t);
            if (e) err[i]++;
            ESP_LOGI("TEMP", "Sensor %d: %.2f °C (errors %d)", i, t, err[i]);
        }
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(TEMP_PERIOD_MS));
    }
}

/* ───────────────────────────────── app_main ───────────────────────────────── */
void app_main(void)
{
    /* LED (optional) */
    led_init();

    /* ── NVS (required by NimBLE) ── */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    /* ── NimBLE stack ── */
    ESP_ERROR_CHECK(nimble_port_init());
    ESP_ERROR_CHECK(gap_init());
    ESP_ERROR_CHECK(gatt_svc_init());
    nimble_host_config_init();

    /* ── Create tasks ── */
    xTaskCreate(nimble_host_task, "NimBLE Host", 4096, NULL, 5, NULL);
    xTaskCreate(heart_rate_task , "Heart Rate" , 4096, NULL, 5, NULL);
    xTaskCreate(ds18b20_task    , "DS18B20"    , 4096, NULL, 5, NULL);
}

/* ───────────────── NimBLE host FreeRTOS wrapper task ─────────────────────── */
static void nimble_host_task(void *param)
{
    ESP_LOGI(TAG, "NimBLE host task running");
    nimble_port_run();      /* blocks until nimble_port_stop() */
    vTaskDelete(NULL);
}