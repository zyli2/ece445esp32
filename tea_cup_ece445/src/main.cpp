// ──────────────────────────────────────────────────────────────────────────────
// main.cpp  –  Arduino core + RTOS tasks (ESP-IDF + Arduino component build)
// ──────────────────────────────────────────────────────────────────────────────
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"
#include "esp_timer.h"

/* ───── Globals shared with the C console ─────────────────────────────────── */
extern "C" {
    float          g_set_point    = 25.0f;
    volatile float g_latest_temp  = 0.0f;
    volatile int   g_heater_state = 0;
    volatile int   g_motor_enabled = 1;
    int64_t        g_start_time_us = 0; // timestamp at startup (microseconds)
}

/* ───── Extern from gpio_example_main.c ────────────────────────────────────── */
extern "C" void console_start(void);
extern SemaphoreHandle_t cli_mutex;

/* ───── Hardware pins ──────────────────────────────────────────────────────── */
#define TEMP_PIN    17
#define HEATER_PIN  18
#define STIR_PIN     4

/* ───── Stirrer cadence (ms) ──────────────────────────────────────────────── */
#define STIR_ON_MS      3000
#define STIR_CYCLE_MS   27000

/* ───── Control hysteresis (°C) ───────────────────────────────────────────── */
#define BAND            0.30f

/* ───── OneWire sensor objects ─────────────────────────────────────────────── */
OneWire           oneWire(TEMP_PIN);
DallasTemperature sensors(&oneWire);

/* ───── Helper to write GPIO quickly ──────────────────────────────────────── */
static inline void gpio_write(gpio_num_t pin, int lvl) { gpio_set_level(pin, lvl); }

/* ───── Motor ON/OFF functions for CLI ────────────────────────────────────── */
extern "C" void motor_on(void)
{
    g_motor_enabled = 1;
    gpio_write((gpio_num_t)STIR_PIN, 1);
}

extern "C" void motor_off(void)
{
    g_motor_enabled = 0;
    gpio_write((gpio_num_t)STIR_PIN, 0);
}

/* ───── Task forward declarations ─────────────────────────────────────────── */
static void temperature_task(void*);
static void heater_task(void*);
static void stirrer_task(void*);
static void telemetry_task(void*);

/* ───── Arduino setup / loop ──────────────────────────────────────────────── */
void setup()
{
    Serial.begin(115200);
    sensors.begin();
    g_start_time_us = esp_timer_get_time();  // record start time

    /* 1 ── create mutex FIRST */
    cli_mutex = xSemaphoreCreateMutex();
    xSemaphoreTake(cli_mutex, portMAX_DELAY);

    /* Configure output pins */
    gpio_config_t io_cfg = {
        .pin_bit_mask = (1ULL << HEATER_PIN) | (1ULL << STIR_PIN),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE
    };
    gpio_config(&io_cfg);

    /* Turn motor ON by default */
    gpio_write((gpio_num_t)STIR_PIN, 1);

    /* FreeRTOS tasks */
    xTaskCreatePinnedToCore(temperature_task, "temp",      15000, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(heater_task,      "heater",    15000, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(stirrer_task,     "stirrer",   15000, NULL, 4, NULL, 1);
    xTaskCreatePinnedToCore(telemetry_task,   "telemetry", 15000, NULL, 3, NULL, 1);

    console_start();
    xSemaphoreGive(cli_mutex);
}

void loop() { vTaskDelay(portMAX_DELAY); }

/* ───── TASK: read DS18B20 every 1 s ───────────────────────────────────────── */
static void temperature_task(void*)
{
    for (;;) {
        sensors.requestTemperatures();
        g_latest_temp = sensors.getTempCByIndex(0);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ───── TASK: bang-bang heater with hysteresis ─────────────────────────────── */
static void heater_task(void*)
{
    for (;;) {
        float t = g_latest_temp;

        if (t < g_set_point - BAND && !g_heater_state) {
            gpio_write((gpio_num_t)HEATER_PIN, 1);
            g_heater_state = 1;
        } else if (t > g_set_point + BAND && g_heater_state) {
            gpio_write((gpio_num_t)HEATER_PIN, 0);
            g_heater_state = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/* ───── TASK: pulse stirrer if enabled ────────────────────────────────────── */
static void stirrer_task(void*)
{
    for (;;) {
        if (g_motor_enabled) {
            gpio_write((gpio_num_t)STIR_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(STIR_ON_MS));
            gpio_write((gpio_num_t)STIR_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(STIR_CYCLE_MS - STIR_ON_MS));
        } else {
            gpio_write((gpio_num_t)STIR_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

/* ───── TASK: print status every 15 s if CLI idle ──────────────────────────── */
static void telemetry_task(void*)
{
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(15000));

        if (cli_mutex && xSemaphoreTake(cli_mutex, 0) == pdTRUE) {
            int64_t now_us = esp_timer_get_time();
            int64_t elapsed_sec = (now_us - g_start_time_us) / 1000000;

            const char *strength = "Unknown";
            if (elapsed_sec < 60) {
                strength = "Brewing";
            } else if (elapsed_sec < 150) {
                strength = "Mild";
            } else if (elapsed_sec < 240) {
                strength = "Medium";
            } else {
                strength = "Strong";
            }

            printf("\r\nT = %.2f °C  |  Set-point = %.2f °C  |  Heater %s  |  Motor %s\r\n",
                   g_latest_temp, g_set_point,
                   g_heater_state ? "ON" : "OFF",
                   g_motor_enabled ? "ON" : "OFF");
            printf("Elapsed Time: %lld sec  |  Strength: %s\r\n", elapsed_sec, strength);

            xSemaphoreGive(cli_mutex);
        }
    }
}

/* ───── Required entry when ESP-IDF is primary framework ───────────────────── */
extern "C" void app_main(void)
{
    initArduino();
    setup();
    for (;;) {
        loop();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
