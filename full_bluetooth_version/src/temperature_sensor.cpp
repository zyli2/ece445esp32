#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "temperature_sensor.h"

static const int oneWireBus = 17;
static OneWire oneWire(oneWireBus);
static DallasTemperature sensors(&oneWire);
static float latest_temperature = -100.0f;

static TaskHandle_t temp_update_task_handle = NULL;

static void temperature_update_task(void *arg) {
    while (true) {
        sensors.requestTemperatures();
        latest_temperature = sensors.getTempCByIndex(0);
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

extern "C" void temperature_sensor_init(void) {
    sensors.begin();
    xTaskCreate(
        temperature_update_task,
        "temperature_update_task",
        20000,
        NULL,
        5,
        &temp_update_task_handle
    );
}

extern "C" float get_latest_temp(void) {
    return latest_temperature;
}