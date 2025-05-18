#ifndef GLOBAL_VARS_H
#define GLOBAL_VARS_H

#include "freertos/FreeRTOS.h"   // For BaseType_t, etc.
#include "freertos/semphr.h"     // For SemaphoreHandle_t
#include "common_types.h"        // For emergency_type_t

// --- Global variables for sensor readings ---
// These are defined in main.c
extern volatile float g_temperature;
extern volatile float g_pressure;
extern volatile float g_humidity;
extern volatile float g_gas;

// --- Global variable for current emergency state ---
// Defined in main.c
extern volatile emergency_type_t g_current_emergency_type;

// --- Mutex for display access and emergency state ---
// Defined in main.c
extern SemaphoreHandle_t g_display_mutex;

#endif // GLOBAL_VARS_H

