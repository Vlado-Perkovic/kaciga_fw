#ifndef SENSOR_LOGIC_H
#define SENSOR_LOGIC_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "global_vars.h" // For global variable extern declarations and common_types.h

// Task function declaration
void sensor_simulation_task(void *pvParameters);

#endif // SENSOR_LOGIC_H

