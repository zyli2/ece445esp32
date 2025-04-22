#include "heater.h"

// static const char *TAG = "turn motor on / off";
// static int heat_state = 0;

 void set_heater_level(int level)
 {
     gpio_set_level(GPIO_OUTPUT_PIN_HEATER, level);
    //  ESP_LOGI(TAG, "Set GPIO %d to level %d", GPIO_OUTPUT_PIN, level);
 }
 
 int heater_on(void)
 {
    if (!heater_configured) {
        configure_heater();
    }
     set_heater_level(1);
     heater_state = 1;
     return 0;
 }
 
 int heater_off(void)
 {
    if (!heater_configured) {
        configure_heater();
    }
     set_heater_level(0);
     heater_state = 0;
     return 0;
 }

 void configure_heater(void) {
    gpio_config_t io_conf = {};
     io_conf.intr_type = GPIO_INTR_DISABLE;
     io_conf.mode = GPIO_MODE_OUTPUT;
     io_conf.pin_bit_mask = 1ULL << GPIO_OUTPUT_PIN_HEATER;
     io_conf.pull_down_en = 0;
     io_conf.pull_up_en = 0;
     gpio_config(&io_conf);
     heater_configured = 1;
 }