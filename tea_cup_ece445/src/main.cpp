// ──────────────────────────────────────────────────────────────────────────────
// main.cpp  –  Arduino core + RTOS tasks  (ESP-IDF + Arduino component build)
// ──────────────────────────────────────────────────────────────────────────────
#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

/* ───── Globals shared with the C console ─────────────────────────────────── */
extern "C" {
    float          g_set_point    = 25.0f;   // °C – CLI can change this
    volatile float g_latest_temp  = 0.0f;    // °C – updated every second
    volatile int   g_heater_state = 0;       // 0 = OFF, 1 = ON
}

/* ───── Extern from gpio_example_main.c ────────────────────────────────────── */
extern "C" void console_start(void);
extern SemaphoreHandle_t cli_mutex;          // mutex defined in C file

/* ───── Hardware pins ──────────────────────────────────────────────────────── */
#define TEMP_PIN    17     // DS18B20
#define HEATER_PIN  18     // MOSFET / relay output (must be output-capable)
#define STIR_PIN     4     // stirrer motor output (change to suit board)

/* ───── Stirrer cadence (ms) ──────────────────────────────────────────────── */
#define STIR_ON_MS      3000
#define STIR_CYCLE_MS   27000

/* ───── Control hysteresis (°C) ───────────────────────────────────────────── */
#define BAND            0.30f   // ±0.30 °C

/* ───── OneWire sensor objects ─────────────────────────────────────────────── */
OneWire           oneWire(TEMP_PIN);
DallasTemperature sensors(&oneWire);

/* ───── Helper to write GPIO quickly ──────────────────────────────────────── */
static inline void gpio_write(gpio_num_t pin, int lvl) { gpio_set_level(pin, lvl); }

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

    /* FreeRTOS tasks */
    xTaskCreatePinnedToCore(temperature_task, "temp",      15000, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(heater_task,      "heater",    15000, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(stirrer_task,     "stirrer",   15000, NULL, 4, NULL, 1);
    xTaskCreatePinnedToCore(telemetry_task,   "telemetry", 15000, NULL, 3, NULL, 1);

    console_start();   // starts CLI task (creates cli_mutex)
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

/* ───── TASK: pulse stirrer (ON 5 s / OFF 55 s) ────────────────────────────── */
static void stirrer_task(void*)
{
    for (;;) {
        gpio_write((gpio_num_t)STIR_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(STIR_ON_MS));
        gpio_write((gpio_num_t)STIR_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(STIR_CYCLE_MS - STIR_ON_MS));
    }
}

/* ───── TASK: print status every 15 s if CLI idle ──────────────────────────── */
static void telemetry_task(void*)
{
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(15000));

        if (cli_mutex &&                         // extra safety
            xSemaphoreTake(cli_mutex, 0) == pdTRUE) {

            printf("\r\nT = %.2f °C  |  Set-point = %.2f °C  |  Heater %s\r\n",
                   g_latest_temp, g_set_point, g_heater_state ? "ON" : "OFF");
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
