#include "motor.h"

// static const char *TAG = "turn motor on / off";
// static int heat_state = 0;

 void set_motor_level(int level)
 {
     gpio_set_level(GPIO_OUTPUT_PIN_MOTOR, level);
    //  ESP_LOGI(TAG, "Set GPIO %d to level %d", GPIO_OUTPUT_PIN, level);
 }
 
 int motor_on(void)
 {
    if (!gpio_configured) {
        configure_gpio();
    }
     set_motor_level(1);
     motor_state = 1;
     return 0;
 }
 
 int motor_off(void)
 {
    if (!gpio_configured) {
        configure_gpio();
    }
     set_motor_level(0);
     motor_state = 0;
     return 0;
 }

 void configure_gpio(void) {
    gpio_config_t io_conf = {};
     io_conf.intr_type = GPIO_INTR_DISABLE;
     io_conf.mode = GPIO_MODE_OUTPUT;
     io_conf.pin_bit_mask = 1ULL << GPIO_OUTPUT_PIN_MOTOR;
     io_conf.pull_down_en = 0;
     io_conf.pull_up_en = 0;
     gpio_config(&io_conf);
     gpio_configured = 1;
 }