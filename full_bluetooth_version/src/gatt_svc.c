#include "gatt_svc.h"
#include "common.h"
#include "motor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "temperature_sensor.h"
#include "temperature_control.h"

static TaskHandle_t s_led_blink_task_handle = NULL;
static bool s_led_blink_active = false;

void gatt_svr_subscribe_cb(struct ble_gap_event *event) {
    ESP_LOGI("GATT", "Received subscription event (conn=%d, attr=%d)",
             event->subscribe.conn_handle, event->subscribe.attr_handle);
}


// ------------------------ Motor Task ------------------------
static void led_blink_task(void *arg)
{
    while (1) {
        if (s_led_blink_active) {
            motor_on();
            bool is_off = false;
            for (int i = 0; i < 3; i++) {
                vTaskDelay(pdMS_TO_TICKS(1000));
                if (!s_led_blink_active) {
                    is_off = true;
                    break;
                }
            }
            if (is_off) {
                continue;
            }
            motor_off();
            vTaskDelay(pdMS_TO_TICKS(27000));
        } else {
            motor_off();
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

// ------------------------ BLE Characteristic Handlers ------------------------
static int led_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg);

static int temp_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg);

static int temp_target_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                  struct ble_gatt_access_ctxt *ctxt, void *arg);

// ------------------------ BLE UUIDs ------------------------
static const ble_uuid16_t auto_io_svc_uuid = BLE_UUID16_INIT(0x1815);

static uint16_t led_chr_val_handle;
static const ble_uuid128_t led_chr_uuid =
    BLE_UUID128_INIT(0x23, 0xd1, 0xbc, 0xea, 0x5f, 0x78, 0x23, 0x15, 0xde, 0xef,
                     0x12, 0x12, 0x25, 0x15, 0x00, 0x00);

static uint16_t temp_chr_val_handle;
static const ble_uuid128_t temp_chr_uuid =
    BLE_UUID128_INIT(0x23, 0xd1, 0xbc, 0xea, 0x5f, 0x78, 0x23, 0x15, 0xde, 0xef,
                     0x12, 0x12, 0x25, 0x15, 0x00, 0x01);

static uint16_t temp_target_chr_val_handle;
static const ble_uuid128_t temp_target_chr_uuid =
    BLE_UUID128_INIT(0x23, 0xd1, 0xbc, 0xea, 0x5f, 0x78, 0x23, 0x15, 0xde, 0xef,
                     0x12, 0x12, 0x25, 0x15, 0x00, 0x02);

// ------------------------ GATT Table ------------------------
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &auto_io_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &led_chr_uuid.u,
                .access_cb = led_chr_access,
                .flags = BLE_GATT_CHR_F_WRITE,
                .val_handle = &led_chr_val_handle
            },
            {
                .uuid = &temp_chr_uuid.u,
                .access_cb = temp_chr_access,
                .flags = BLE_GATT_CHR_F_READ,
                .val_handle = &temp_chr_val_handle
            },
            {
                .uuid = &temp_target_chr_uuid.u,
                .access_cb = temp_target_chr_access,
                .flags = BLE_GATT_CHR_F_WRITE,
                .val_handle = &temp_target_chr_val_handle
            },
            { 0 }
        },
    },
    { 0 }
};

// ------------------------ Characteristic Access ------------------------
static int led_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR && attr_handle == led_chr_val_handle) {
        if (ctxt->om->om_len == 1) {
            if (ctxt->om->om_data[0]) {
                s_led_blink_active = true;
                ESP_LOGI(TAG, "Motor ON cycle enabled.");
            } else {
                s_led_blink_active = false;
                motor_off();
                ESP_LOGI(TAG, "Motor OFF.");
            }
            return 0;
        }
    }

    ESP_LOGE(TAG, "unexpected write or length in led_chr_access");
    return BLE_ATT_ERR_UNLIKELY;
}

static int temp_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR && attr_handle == temp_chr_val_handle) {
        float temp = get_latest_temp();
        uint16_t temp_int = (uint16_t)(temp * 100);
        int rc = os_mbuf_append(ctxt->om, &temp_int, sizeof(temp_int));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    ESP_LOGE(TAG, "unexpected access in temp_chr_access");
    return BLE_ATT_ERR_UNLIKELY;
}

static int temp_target_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                  struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR &&
        attr_handle == temp_target_chr_val_handle) {

        if (ctxt->om->om_len == 2) {
            uint16_t raw = *(uint16_t *)ctxt->om->om_data;
            float target = (float)raw / 100.0f;
            temperature_control_set_target(target);
            ESP_LOGI(TAG, "Received target temperature: %.2f °C", target);
            return 0;
        } else {
            ESP_LOGE(TAG, "Invalid target temp length: %d", ctxt->om->om_len);
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
    }

    ESP_LOGE(TAG, "unexpected access in temp_target_chr_access");
    return BLE_ATT_ERR_UNLIKELY;
}

// ------------------------ Registration Callback ------------------------
void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg) {
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
        ESP_LOGD(TAG, "registered service %s with handle=%d",
                 ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf), ctxt->svc.handle);
        break;
    case BLE_GATT_REGISTER_OP_CHR:
        ESP_LOGD(TAG, "registered characteristic %s (def=%d, val=%d)",
                 ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                 ctxt->chr.def_handle, ctxt->chr.val_handle);
        break;
    case BLE_GATT_REGISTER_OP_DSC:
        ESP_LOGD(TAG, "registered descriptor %s with handle=%d",
                 ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf), ctxt->dsc.handle);
        break;
    default:
        assert(0);
        break;
    }
}

// ------------------------ Init ------------------------
int gatt_svc_init(void) {
    int rc;

    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) return rc;

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) return rc;

    if (s_led_blink_task_handle == NULL) {
        xTaskCreate(led_blink_task, "led_blink_task", 20000, NULL, 5, &s_led_blink_task_handle);
    }

    temperature_sensor_init();
    temperature_control_init();

    return 0;
}