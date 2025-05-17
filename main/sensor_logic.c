#include "sensor_logic.h" // Includes global_vars.h and common_types.h
#include <stdio.h>
#include "esp_log.h"
// freertos/semphr.h is included via global_vars.h -> freertos/FreeRTOS.h or directly if needed

static const char *TAG = "SENSOR_LOGIC";

// --- Configuration (copied from original main.c, can be centralized if needed) ---
#define SENSOR_UPDATE_INTERVAL_MS 5000
#define EMERGENCY_SIM_INTERVAL_COUNT 3
#define EMERGENCY_DURATION_SENSOR_CYCLES 2

void sensor_simulation_task(void *pvParameters) {
    uint8_t trigger_counter = 0;
    uint8_t emergency_active_duration_counter = 0;
    emergency_type_t next_emergency_to_simulate = EMERGENCY_TYPE_DANGER;

    ESP_LOGI(TAG, "Sensor simulation task started.");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(SENSOR_UPDATE_INTERVAL_MS));

        g_temperature += 0.5f;
        if (g_temperature > 40.0f) g_temperature = 20.0f;
        g_pressure -= 1.0f;
        if (g_pressure < 980.0f) g_pressure = 1012.5f;
        g_humidity += 2.0f;
        if (g_humidity > 90.0f) g_humidity = 50.0f;

        ESP_LOGI(TAG, "Simulated sensor update: T=%.1f, P=%.1f, H=%.0f", g_temperature, g_pressure, g_humidity);

        if (xSemaphoreTake(g_display_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (emergency_active_duration_counter > 0) {
                emergency_active_duration_counter--;
                if (emergency_active_duration_counter == 0) {
                    ESP_LOGI(TAG, "Clearing simulated emergency.");
                    g_current_emergency_type = EMERGENCY_TYPE_NONE;
                }
            } else {
                trigger_counter++;
                if (trigger_counter >= EMERGENCY_SIM_INTERVAL_COUNT) {
                    trigger_counter = 0;
                    if (g_current_emergency_type == EMERGENCY_TYPE_NONE) {
                        g_current_emergency_type = next_emergency_to_simulate;
                        emergency_active_duration_counter = EMERGENCY_DURATION_SENSOR_CYCLES;
                        ESP_LOGW(TAG, "Simulating EMERGENCY: %s for %d sensor cycles",
                                 (g_current_emergency_type == EMERGENCY_TYPE_DANGER) ? "DANGER" : "FALL",
                                 EMERGENCY_DURATION_SENSOR_CYCLES);

                        if (next_emergency_to_simulate == EMERGENCY_TYPE_DANGER) {
                            next_emergency_to_simulate = EMERGENCY_TYPE_FALL;
                        } else {
                            next_emergency_to_simulate = EMERGENCY_TYPE_DANGER;
                        }
                    }
                }
            }
            xSemaphoreGive(g_display_mutex);
        } else {
            ESP_LOGW(TAG, "Sensor task could not acquire mutex for emergency simulation.");
        }
    }
}

