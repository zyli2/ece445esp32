#ifndef TEMPERATURE_SENSOR_H
#define TEMPERATURE_SENSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void temperature_sensor_init(void);
float get_latest_temp(void);

#ifdef __cplusplus
}
#endif

#endif