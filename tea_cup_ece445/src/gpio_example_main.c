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

#define GPIO_OUTPUT_PIN 18
static const char *TAG = "heater_cli";
static int heat_state = 0;
static float temp_threshold = 100.0; // Set high so heater stays off until user sets

// Link to C++ temperature function
extern float get_latest_temp(void);

static void set_gpio_level(int level)
{
    gpio_set_level(GPIO_OUTPUT_PIN, level);
    ESP_LOGI(TAG, "Set GPIO %d to level %d", GPIO_OUTPUT_PIN, level);
}

static void heater_control_task(void *arg)
{
    while (1) {
        float temp = get_latest_temp();
        ESP_LOGI(TAG, "Current Temp: %.2f °C | Threshold: %.2f °C", temp, temp_threshold);

        if (temp < temp_threshold && heat_state == 0) {
            set_gpio_level(1);
            heat_state = 1;
            ESP_LOGI(TAG, "Heater ON");
        } else if (temp >= temp_threshold && heat_state == 1) {
            set_gpio_level(0);
            heat_state = 0;
            ESP_LOGI(TAG, "Heater OFF");
        }

        vTaskDelay(pdMS_TO_TICKS(15000)); // every 15 seconds
    }
}

static int cmd_set_temp(int argc, char **argv)
{
    if (argc != 2) {
        ESP_LOGW(TAG, "Usage: set <temperature>");
        return 1;
    }
    temp_threshold = atof(argv[1]);
    ESP_LOGI(TAG, "Temperature threshold set to %.2f °C", temp_threshold);
    return 0;
}

static void gpio_console_loop(void *arg)
{
    ESP_LOGI(TAG, "Type 'set <temp>' to set heater threshold. Current=%.2f °C", temp_threshold);

    while (1) {
        char *line = linenoise("cmd> ");
        if (!line) continue;
        if (strlen(line) > 0) linenoiseHistoryAdd(line);
        int ret = esp_console_run(line, NULL);
        if (ret != ESP_OK) {
            ESP_LOGI(TAG, "Command returned error: %d", ret);
        }
        linenoiseFree(line);
    }
}

void gpio_console_start(void)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << GPIO_OUTPUT_PIN,
        .pull_down_en = 0,
        .pull_up_en = 0,
    };
    gpio_config(&io_conf);

    const uart_config_t uart_cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_XTAL,
    };
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uart_cfg));
    esp_vfs_dev_uart_use_driver(UART_NUM_0);

    esp_console_config_t console_cfg = {
        .max_cmdline_length = 256,
        .max_cmdline_args = 8,
#if CONFIG_LOG_COLORS
        .hint_color = atoi(LOG_COLOR_CYAN),
#endif
        .hint_bold = 1
    };
    ESP_ERROR_CHECK(esp_console_init(&console_cfg));

    linenoiseSetMultiLine(1);
    linenoiseHistorySetMaxLen(50);

    const esp_console_cmd_t set_temp_cmd = {
        .command = "set",
        .help = "Set temperature threshold for heater control",
        .hint = NULL,
        .func = &cmd_set_temp,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&set_temp_cmd));

    ESP_LOGI(TAG, "CLI + heater control tasks starting…");
    xTaskCreate(gpio_console_loop, "gpio_cli", 4096, NULL, 5, NULL);
    xTaskCreate(heater_control_task, "heater_control", 2048, NULL, 5, NULL);
}
