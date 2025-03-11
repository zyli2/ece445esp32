/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "led_strip.h"
#include "sdkconfig.h"
#include "string.h"
#include "stdlib.h"
#include "esp_system.h"
#include "esp_console.h"
#include "esp_vfs_dev.h"
#include "driver/uart.h"
#include "linenoise/linenoise.h"

static const char *TAG = "example";

/* Use project configuration menu (idf.py menuconfig) to choose the GPIO to blink,
   or you can edit the following line and set a number here.
*/
#define BLINK_GPIO CONFIG_BLINK_GPIO

static uint8_t s_led_state = 0;

#ifdef CONFIG_BLINK_LED_STRIP

static led_strip_handle_t led_strip;

static void blink_led(void)
{
    /* If the addressable LED is enabled */
    if (s_led_state) {
        /* Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each color */
        led_strip_set_pixel(led_strip, 0, 16, 16, 16);
        /* Refresh the strip to send data */
        led_strip_refresh(led_strip);
    } else {
        /* Set all LED off to clear all pixels */
        // led_strip_clear(led_strip);
    }
}

static void configure_led(void)
{
    ESP_LOGI(TAG, "Example configured to blink addressable LED!");
    /* LED strip initialization with the GPIO and pixels number*/
    led_strip_config_t strip_config = {
        .strip_gpio_num = BLINK_GPIO,
        .max_leds = 1, // at least one LED on board
    };
#if CONFIG_BLINK_LED_STRIP_BACKEND_RMT
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
#elif CONFIG_BLINK_LED_STRIP_BACKEND_SPI
    led_strip_spi_config_t spi_config = {
        .spi_bus = SPI2_HOST,
        .flags.with_dma = true,
    };
    ESP_ERROR_CHECK(led_strip_new_spi_device(&strip_config, &spi_config, &led_strip));
#else
#error "unsupported LED strip backend"
#endif
    /* Set all LED off to clear all pixels */
    led_strip_clear(led_strip);
}

#elif CONFIG_BLINK_LED_GPIO

static void blink_led(void)
{
    /* Set the GPIO level according to the state (LOW or HIGH)*/
    gpio_set_level(BLINK_GPIO, s_led_state);
}

static void configure_led(void)
{
    ESP_LOGI(TAG, "Example configured to blink GPIO LED!");
    gpio_reset_pin(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

#else
#error "unsupported LED type"
#endif

// void app_main(void)
// {

//     /* Configure the peripheral according to the LED type */
//     configure_led();

//     while (1) {
//         ESP_LOGI(TAG, "Turning the LED %s!", s_led_state == true ? "ON" : "OFF");
//         blink_led();
//         /* Toggle the LED state */
//         s_led_state = !s_led_state;
//         vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);
//     }
// }

void app_main(void)
{
    int cmd_led_on(int argc, char **argv)
    {
        s_led_state = 1;
        blink_led();
        ESP_LOGI(TAG, "LED turned ON");
        return 0; // success
    }

    int cmd_led_off(int argc, char **argv)
    {
        s_led_state = 0;
        blink_led();
        ESP_LOGI(TAG, "LED turned OFF");
        return 0; // success
    }

    configure_led();
    ESP_LOGI(TAG, "LED configured. Console-based control starting...");

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
        const esp_console_cmd_t led_on_cmd = {
            .command = "led_on",
            .help = "Turn the LED on",
            .hint = NULL,
            .func = &cmd_led_on,
        };
        ESP_ERROR_CHECK(esp_console_cmd_register(&led_on_cmd));

        const esp_console_cmd_t led_off_cmd = {
            .command = "led_off",
            .help = "Turn the LED off",
            .hint = NULL,
            .func = &cmd_led_off,
        };
        ESP_ERROR_CHECK(esp_console_cmd_register(&led_off_cmd));
    }

    ESP_LOGI(TAG, "Type 'led_on' or 'led_off' and press ENTER to control the LED.");

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
            ESP_LOGW(TAG, "Command returned an error: %d", ret);
        }

        linenoiseFree(line);
    }
}