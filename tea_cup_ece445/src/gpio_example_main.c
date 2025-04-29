// ──────────────────────────────────────────────────────────────────────────────
// gpio_example_main.c  –  Serial CLI using esp_console + linenoise
// ──────────────────────────────────────────────────────────────────────────────
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_console.h"
#include "linenoise/linenoise.h"
#include "esp_vfs_dev.h"
#include "esp_timer.h"

/* ───── Shared variables from main.cpp ─────────────────────────────────────── */
extern float          g_set_point;
extern volatile float g_latest_temp;
extern volatile int   g_heater_state;
extern volatile int   g_motor_enabled;
extern int64_t        g_start_time_us;

/* ───── External motor control ─────────────────────────────────────────────── */
extern void motor_on(void);
extern void motor_off(void);

/* ───── Global mutex used by console & telemetry ───────────────────────────── */
SemaphoreHandle_t cli_mutex = NULL;

/* ───── Command: set <temperature> ─────────────────────────────────────────── */
static int cmd_set(int argc, char **argv)
{
    if (argc < 2 || argv[1] == NULL) {
        printf("Usage: set <temperature °C>\r\n");
        return 1;
    }
    char *endp;
    float val = strtof(argv[1], &endp);
    if (endp == argv[1]) {
        printf("Invalid number.\r\n");
        return 1;
    }
    g_set_point = val;
    printf("New set-point: %.2f °C\r\n", g_set_point);
    return 0;
}

/* ───── Command: status ────────────────────────────────────────────────────── */
static int cmd_status(int argc, char **argv)
{
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

    printf("T = %.2f °C, set-point = %.2f °C, heater %s, motor %s\r\n",
           g_latest_temp, g_set_point,
           g_heater_state ? "ON" : "OFF",
           g_motor_enabled ? "ON" : "OFF");
    printf("Elapsed Time: %lld sec  |  Strength: %s\r\n", elapsed_sec, strength);
    return 0;
}

/* ───── Command: motor_on ──────────────────────────────────────────────────── */
static int cmd_motor_on(int argc, char **argv)
{
    motor_on();
    printf("Motor turned ON manually.\r\n");
    return 0;
}

/* ───── Command: motor_off ─────────────────────────────────────────────────── */
static int cmd_motor_off(int argc, char **argv)
{
    motor_off();
    printf("Motor turned OFF manually.\r\n");
    return 0;
}

/* ───── CLI task ───────────────────────────────────────────────────────────── */
static void console_task(void *arg)
{
    esp_console_config_t cfg = {
        .max_cmdline_args   = 8,
        .max_cmdline_length = 256,
    };
    esp_console_init(&cfg);

    const esp_console_cmd_t c_set  = { .command = "set",       .help = "Set temperature °C", .func = &cmd_set };
    const esp_console_cmd_t c_st   = { .command = "status",    .help = "Show current state",  .func = &cmd_status };
    const esp_console_cmd_t c_mon  = { .command = "motor_on",  .help = "Turn motor ON",        .func = &cmd_motor_on };
    const esp_console_cmd_t c_moff = { .command = "motor_off", .help = "Turn motor OFF",       .func = &cmd_motor_off };

    esp_console_cmd_register(&c_set);
    esp_console_cmd_register(&c_st);
    esp_console_cmd_register(&c_mon);
    esp_console_cmd_register(&c_moff);

    linenoiseSetMultiLine(1);
    linenoiseSetDumbMode(1);
    linenoiseHistorySetMaxLen(50);

    printf("\r\nType 'set <temp>', 'status', 'motor_on', 'motor_off'.\r\n");

    while (true) {
        xSemaphoreTake(cli_mutex, portMAX_DELAY);
        char *line = linenoise("cmd> ");
        xSemaphoreGive(cli_mutex);

        if (!line) continue;
        if (strlen(line)) linenoiseHistoryAdd(line);
        int tmp;
        esp_console_run(line, &tmp);
        linenoiseFree(line);
    }
}

/* ───── Public entry called from main.cpp ──────────────────────────────────── */
void console_start(void)
{
    uart_driver_install(UART_NUM_0, 1024, 0, 0, NULL, 0);
    esp_vfs_dev_uart_use_driver(UART_NUM_0);

    if (cli_mutex == NULL) {
        cli_mutex = xSemaphoreCreateMutex();
        configASSERT(cli_mutex);
    }

    xTaskCreatePinnedToCore(console_task, "console", 20000, NULL, 5, NULL, 0);
}
