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
#include "stdbool.h"

#define GPIO_OUTPUT_PIN_MOTOR 18
static int motor_state = 0;
static int gpio_configured = 0;

void set_gpio_level(int level);
int cmd_gpio_on(void);
int cmd_gpio_off(void);
void configure_gpio(void);