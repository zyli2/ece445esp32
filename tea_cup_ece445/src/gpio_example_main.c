/*
 * Simple CLI that toggles GPIO_OUTPUT_PIN with "on" / "off".
 * Build as C so the symbol name is unmangled for C++ callers.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "linenoise/linenoise.h"
#include "driver/uart.h"

/* -------- configurable bits ------------------------------------------- */
#define GPIO_OUTPUT_PIN 18
/* ---------------------------------------------------------------------- */

static const char *TAG = "heater_cli";
static int heat_state = 0;

/* ---------- helpers --------------------------------------------------- */
static void set_gpio_level(int level)
{
    gpio_set_level(GPIO_OUTPUT_PIN, level);
    ESP_LOGI(TAG, "Set GPIO %d to level %d", GPIO_OUTPUT_PIN, level);
}

static int cmd_gpio_on(int argc, char **argv)
{
    set_gpio_level(1);
    heat_state = 1;
    return 0;
}

static int cmd_gpio_off(int argc, char **argv)
{
    set_gpio_level(0);
    heat_state = 0;
    return 0;
}
/* ---------------------------------------------------------------------- */

/* Interactive loop; runs inside its own FreeRTOS task */
static void gpio_console_loop(void *arg)
{
    ESP_LOGI(TAG, "Type 'on' or 'off' then ENTER to control GPIO. Current=%d",
             heat_state);

    while (true) {
        char *line = linenoise("cmd> ");
        if (!line) {
            continue;
        }
        if (strlen(line) > 0) {
            linenoiseHistoryAdd(line);
        }
        int ret = esp_console_run(line, NULL);
        if (ret != ESP_OK) {
            ESP_LOGI(TAG, "Command returned err: %d", ret);
        }
        linenoiseFree(line);
    }
}

/* ----------- public API ---------------------------------------------- */
void gpio_console_start(void)
{
    /* 1. GPIO config */
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << GPIO_OUTPUT_PIN,
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);

    /* 2. UART for console I/O */
    const uart_config_t uart_cfg = {
        .baud_rate  = 115200,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_XTAL,      // OK for ESP32‑S3
    };
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uart_cfg));
    esp_vfs_dev_uart_use_driver(UART_NUM_0);

    /* 3. esp_console setup */
    esp_console_config_t console_cfg = {
        .max_cmdline_length = 256,
        .max_cmdline_args   = 8,
#if CONFIG_LOG_COLORS
        .hint_color = atoi(LOG_COLOR_CYAN),
#endif
        .hint_bold = 1
    };
    ESP_ERROR_CHECK(esp_console_init(&console_cfg));

    linenoiseSetMultiLine(1);
    linenoiseHistorySetMaxLen(50);

    /* 4. Register commands */
    const esp_console_cmd_t gpio_on_cmd = {
        .command = "on",
        .help    = "Set the GPIO pin HIGH",
        .hint    = NULL,
        .func    = &cmd_gpio_on,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&gpio_on_cmd));

    const esp_console_cmd_t gpio_off_cmd = {
        .command = "off",
        .help    = "Set the GPIO pin LOW",
        .hint    = NULL,
        .func    = &cmd_gpio_off,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&gpio_off_cmd));

    ESP_LOGI(TAG, "CLI task starting …");

    /* 5. Launch the REPL on its own FreeRTOS task */
    xTaskCreate(gpio_console_loop, "gpio_cli", 4096, NULL, 5, NULL);
}
