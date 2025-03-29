// #include "ds18b20.h"
// #include "driver/gpio.h"
// #include "esp_log.h"
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include <rom/ets_sys.h>

// static const char *TAG = "DS18B20";
// static gpio_num_t ds18b20_pin = GPIO_NUM_17; // change to the correct GPIO

// // Enable "strong pull-up" by temporarily driving the pin HIGH in push-pull
// static void ds18b20_enable_strong_pullup(void)
// {
//     gpio_set_direction(ds18b20_pin, GPIO_MODE_OUTPUT);
//     gpio_set_level(ds18b20_pin, 1);
// }

// // Disable "strong pull-up," return to open-drain mode with pull-up
// static void ds18b20_disable_strong_pullup(void)
// {
//     gpio_set_direction(ds18b20_pin, GPIO_MODE_INPUT_OUTPUT_OD);
//     gpio_set_pull_mode(ds18b20_pin, GPIO_PULLUP_ONLY);
// }

// // Initialize DS18B20 for parasitic mode
// esp_err_t ds18b20_init(gpio_num_t pin)
// {
//     ds18b20_pin = pin;
//     gpio_reset_pin(ds18b20_pin);
//     gpio_set_direction(ds18b20_pin, GPIO_MODE_INPUT_OUTPUT_OD);
//     gpio_set_pull_mode(ds18b20_pin, GPIO_PULLUP_ONLY);
//     return ESP_OK;
// }

// static esp_err_t ds18b20_reset(void)
// {
//     gpio_set_level(ds18b20_pin, 0);
//     ets_delay_us(480);
//     gpio_set_level(ds18b20_pin, 1);
//     ets_delay_us(70);

//     int presence = gpio_get_level(ds18b20_pin);
//     ets_delay_us(410);

//     // Typically, presence=0 means sensor is present
//     if (presence == 0) {
//         return ESP_OK;
//     }
//     return ESP_FAIL;
// }

// static void ds18b20_write_bit(uint8_t bit)
// {
//     gpio_set_level(ds18b20_pin, 0);
//     if (bit) {
//         ets_delay_us(10);
//         gpio_set_level(ds18b20_pin, 1);
//         ets_delay_us(55);
//     } else {
//         ets_delay_us(65);
//         gpio_set_level(ds18b20_pin, 1);
//         ets_delay_us(5);
//     }
// }

// static uint8_t ds18b20_read_bit(void)
// {
//     uint8_t bit;
//     gpio_set_level(ds18b20_pin, 0);
//     ets_delay_us(3);
//     gpio_set_level(ds18b20_pin, 1);
//     ets_delay_us(10);
//     bit = gpio_get_level(ds18b20_pin);
//     ets_delay_us(50);
//     return bit;
// }

// static void ds18b20_write_byte(uint8_t byte)
// {
//     for (int i = 0; i < 8; i++) {
//         ds18b20_write_bit(byte & 0x01);
//         byte >>= 1;
//     }
// }

// static uint8_t ds18b20_read_byte(void)
// {
//     uint8_t byte = 0;
//     for (int i = 0; i < 8; i++) {
//         byte |= (ds18b20_read_bit() << i);
//     }
//     return byte;
// }

// esp_err_t ds18b20_read_temperature(float *temperature)
// {
//     // 1) Reset, skip ROM, convert T
//     if (ds18b20_reset() != ESP_OK) {
//         ESP_LOGE(TAG, "Reset failed. Sensor not present.");
//         return ESP_FAIL;
//     }
//     ds18b20_write_byte(0xCC); // Skip ROM
//     ds18b20_write_byte(0x44); // Convert T

//     // 2) Strong pull-up for the duration of the conversion (parasitic mode)
//     ds18b20_enable_strong_pullup();
//     // Wait up to 750ms for 12-bit
//     vTaskDelay(pdMS_TO_TICKS(750));
//     ds18b20_disable_strong_pullup();

//     // 3) Reset, skip ROM, read scratchpad
//     if (ds18b20_reset() != ESP_OK) {
//         ESP_LOGE(TAG, "Reset failed after conversion.");
//         return ESP_FAIL;
//     }
//     ds18b20_write_byte(0xCC); // Skip ROM
//     ds18b20_write_byte(0xBE); // Read Scratchpad

//     uint8_t scratchpad[9];
//     for (int i = 0; i < 9; i++) {
//         scratchpad[i] = ds18b20_read_byte();
//     }

//     int16_t raw_temp = scratchpad[0] | (scratchpad[1] << 8);
//     *temperature = raw_temp * 0.0625f;

//     return ESP_OK;
// }