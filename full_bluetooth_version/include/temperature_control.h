#ifndef TEMPERATURE_CONTROL_H
#define TEMPERATURE_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

void temperature_control_init(void);
void temperature_control_set_target(float target);
float temperature_control_get_target(void);

#ifdef __cplusplus
}
#endif

#endif