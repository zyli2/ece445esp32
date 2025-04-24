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

#define GPIO_OUTPUT_PIN_HEATER 18
static int heater_state = 0;
static int heater_configured = 0;

void set_heater_level(int level);
int heater_on(void);
int heater_off(void);
void configure_heater(void);