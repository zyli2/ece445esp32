/*
 * SPDX-FileCopyrightText: 2020-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

 #include <stdio.h>
 #include <string.h>
 #include <stdlib.h>
 #include <inttypes.h>
 #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"
 #include "freertos/queue.h"
 #include "driver/gpio.h"
 #include "esp_system.h"
 #include "esp_log.h"
 #include "esp_console.h"
 #include "esp_vfs_dev.h"
 #include "linenoise/linenoise.h"
 #include "driver/uart.h"
 
#define GPIO_OUTPUT_PIN 18

 static void set_gpio_level(int level)
 {
     gpio_set_level(GPIO_OUTPUT_PIN, level);
     ESP_LOGI(TAG, "Set GPIO %d to level %d", GPIO_OUTPUT_PIN, level);
 }
 
 static int cmd_gpio_on(int argc, char **argv)
 {
     set_gpio_level(1);
     return 0;
 }
 
 static int cmd_gpio_off(int argc, char **argv)
 {
     set_gpio_level(0);
     return 0;
 }
 
 void app_main(void)
 {
     gpio_config_t io_conf = {};
     io_conf.intr_type = GPIO_INTR_DISABLE;
     io_conf.mode = GPIO_MODE_OUTPUT;
     io_conf.pin_bit_mask = 1ULL << GPIO_OUTPUT_PIN;
     io_conf.pull_down_en = 0;
     io_conf.pull_up_en = 0;
     gpio_config(&io_conf);
 
     ESP_LOGI(TAG, "Configured GPIO %d as output. Starting console...", GPIO_OUTPUT_PIN);

     {
         const uart_config_t uart_config = {
             .baud_rate = 115200,
             .data_bits = UART_DATA_8_BITS,
             .parity    = UART_PARITY_DISABLE,
             .stop_bits = UART_STOP_BITS_1,
             .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
             .source_clk = UART_SCLK_DEFAULT,
         };
         ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0));
         ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uart_config));
         esp_vfs_dev_uart_use_driver(UART_NUM_0);
     }

     {
         esp_console_config_t console_config = {
             .max_cmdline_length = 256,
             .max_cmdline_args = 8,
 #if CONFIG_LOG_COLORS
             .hint_color = atoi(LOG_COLOR_CYAN),
 #endif
             .hint_bold = 1
         };
         ESP_ERROR_CHECK(esp_console_init(&console_config));
     }

     linenoiseSetMultiLine(1);
     linenoiseHistorySetMaxLen(50);

     {
         const esp_console_cmd_t gpio_on_cmd = {
             .command = "on",
             .help = "Set the GPIO pin HIGH",
             .hint = NULL,
             .func = &cmd_gpio_on,
         };
         ESP_ERROR_CHECK(esp_console_cmd_register(&gpio_on_cmd));
 
         const esp_console_cmd_t gpio_off_cmd = {
             .command = "off",
             .help = "Set the GPIO pin LOW",
             .hint = NULL,
             .func = &cmd_gpio_off,
         };
         ESP_ERROR_CHECK(esp_console_cmd_register(&gpio_off_cmd));
     }
 
     ESP_LOGI(TAG, "Type 'gpio_on' or 'gpio_off' and press ENTER to control the output.");

     while (1) {
         char *line = linenoise("cmd> ");
         if (line == NULL) {
             continue;
         }
         if (strlen(line) > 0) {
             linenoiseHistoryAdd(line);
         }
 
         int ret = esp_console_run(line, NULL);
         if (ret != ESP_OK) {
             ESP_LOGI(TAG, "Command returned error code: %d", ret);
         }
 
         linenoiseFree(line);
     }
 }