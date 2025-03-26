#include "ds18b20.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "DS18B20";
static gpio_num_t ds18b20_pin = GPIO_NUM_NC; // need to change to right GPIO number

/* Initialize the DS18B20 sensor by configuring the GPIO pin */
esp_err_t ds18b20_init(gpio_num_t pin) {
    ds18b20_pin = pin;
    gpio_reset_pin(ds18b20_pin);
    // Set as open-drain with an internal pull-up resistor
    gpio_set_direction(ds18b20_pin, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_pull_mode(ds18b20_pin, GPIO_PULLUP_ONLY);
    return ESP_OK;
}

/* Send a reset pulse and check for the sensor's presence */
static esp_err_t ds18b20_reset(void) {
    // Pull bus low for at least 480µs
    gpio_set_level(ds18b20_pin, 0);
    ets_delay_us(480);

    // Release the bus
    gpio_set_level(ds18b20_pin, 1);
    ets_delay_us(70);

    // Read the presence pulse from the sensor (should be low)
    int presence = gpio_get_level(ds18b20_pin);
    ets_delay_us(410);

    if (presence == 0) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

/* Write a single bit to the DS18B20 */
static void ds18b20_write_bit(uint8_t bit) {
    gpio_set_level(ds18b20_pin, 0);
    if (bit) {
        ets_delay_us(10);    // For writing a '1', hold low for ~10µs
        gpio_set_level(ds18b20_pin, 1);
        ets_delay_us(55);    // Total time slot ~65µs
    } else {
        ets_delay_us(65);    // For writing a '0', hold low for ~65µs
        gpio_set_level(ds18b20_pin, 1);
        ets_delay_us(5);
    }
}

/* Read a single bit from the DS18B20 */
static uint8_t ds18b20_read_bit(void) {
    uint8_t bit;
    gpio_set_level(ds18b20_pin, 0);
    ets_delay_us(3);
    gpio_set_level(ds18b20_pin, 1);
    ets_delay_us(10);   // Wait before sampling the bus
    bit = gpio_get_level(ds18b20_pin);
    ets_delay_us(50);   // Wait for rest of the time slot
    return bit;
}

/* Write a byte to the DS18B20, LSB first */
static void ds18b20_write_byte(uint8_t byte) {
    for (int i = 0; i < 8; i++) {
        ds18b20_write_bit(byte & 0x01);
        byte >>= 1;
    }
}

/* Read a byte from the DS18B20, LSB first */
static uint8_t ds18b20_read_byte(void) {
    uint8_t byte = 0;
    for (int i = 0; i < 8; i++) {
        byte |= (ds18b20_read_bit() << i);
    }
    return byte;
}

/* Read temperature from the DS18B20 sensor */
esp_err_t ds18b20_read_temperature(float *temperature) {
    // Start temperature conversion
    if (ds18b20_reset() != ESP_OK) {
        ESP_LOGE(TAG, "Reset failed. Sensor not present.");
        return ESP_FAIL;
    }
    ds18b20_write_byte(0xCC); // Skip ROM command
    ds18b20_write_byte(0x44); // Convert T command

    // Wait for conversion (max 750ms for 12-bit resolution)
    vTaskDelay(pdMS_TO_TICKS(750));

    // Read the scratchpad
    if (ds18b20_reset() != ESP_OK) {
        ESP_LOGE(TAG, "Reset failed after conversion.");
        return ESP_FAIL;
    }
    ds18b20_write_byte(0xCC); // Skip ROM command
    ds18b20_write_byte(0xBE); // Read Scratchpad command

    uint8_t scratchpad[9];
    for (int i = 0; i < 9; i++) {
        scratchpad[i] = ds18b20_read_byte();
    }

    // Combine the first two bytes (LSB first) to get the raw temperature reading
    int16_t raw_temp = scratchpad[0] | (scratchpad[1] << 8);
    *temperature = raw_temp * 0.0625; // Each bit represents 0.0625°C at 12-bit resolution

    return ESP_OK;
}
