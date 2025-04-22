#include "temperature_control.h"
#include "temperature_sensor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "heater.h"
#include "esp_log.h"

static const char *TAG = "temp_ctrl";

static float target_temperature = -100.0f;
static TaskHandle_t temp_control_task_handle = NULL;

void temperature_control_set_target(float target) {
    target_temperature = target;
    ESP_LOGI(TAG, "Target temperature set to: %.2f °C", target);
}

float temperature_control_get_target(void) {
    return target_temperature;
}

static void temperature_control_task(void *arg) {
    while (1) {
        float current_temp = get_latest_temp();
        float target = target_temperature;

        if (target != -100.0f) {
            if (current_temp < target - 0.5f) {
                heater_on();
                ESP_LOGI(TAG, "Heater ON (current=%.2f < target=%.2f)", current_temp, target);
            } else if (current_temp > target + 0.5f) {
                heater_off();
                ESP_LOGI(TAG, "Heater OFF (current=%.2f > target=%.2f)", current_temp, target);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void temperature_control_init(void) {
    if (temp_control_task_handle == NULL) {
        xTaskCreate(
            temperature_control_task,
            "temperature_control_task",
            20000,
            NULL,
            5,
            &temp_control_task_handle
        );
    }
}
