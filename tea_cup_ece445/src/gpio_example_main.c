// ──────────────────────────────────────────────────────────────────────────────
// gpio_example_main.c  –  Serial CLI using esp_console + linenoise
// ──────────────────────────────────────────────────────────────────────────────
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include "esp_console.h"
#include "linenoise/linenoise.h"
#include "esp_vfs_dev.h"

/* ───── Shared variables from main.cpp ─────────────────────────────────────── */
extern float          g_set_point;
extern volatile float g_latest_temp;
extern volatile int   g_heater_state;

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
    printf("T = %.2f °C, set-point = %.2f °C, heater %s\r\n",
           g_latest_temp, g_set_point, g_heater_state ? "ON" : "OFF");
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

    const esp_console_cmd_t c_set = { .command = "set",    .help = "Set temperature °C", .func = &cmd_set };
    const esp_console_cmd_t c_st  = { .command = "status", .help = "Show current state", .func = &cmd_status };
    esp_console_cmd_register(&c_set);
    esp_console_cmd_register(&c_st);

    /* ─── prompt configuration ─────────────────────────────────────────── */
    linenoiseSetMultiLine(1);      // allow long lines to wrap
    linenoiseSetDumbMode(1);       // <<<<< prevents ANSI escape codes
    linenoiseHistorySetMaxLen(50);
    printf("\r\nType 'set <temp>' or 'status'.\r\n");

    while (true) {
        xSemaphoreTake(cli_mutex, portMAX_DELAY);   // lock while user edits line
        char *line = linenoise("cmd> ");
        xSemaphoreGive(cli_mutex);                  // release so telemetry can print

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

    if (cli_mutex == NULL) {                     // ← create the mutex **before**
        cli_mutex = xSemaphoreCreateMutex();     //    the task starts using it
        configASSERT(cli_mutex);                 //    (halts on allocation failure)
    }

    xTaskCreatePinnedToCore(                     // optional: pin to core 0
            console_task, "console",
            20000 /* plenty for esp_console + linenoise */,
            NULL, 5, NULL, 0);
        
}
