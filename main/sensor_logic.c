#include "sensor.h"
#include "sensor_logic.h" // Includes global_vars.h and common_types.h
#include <stdio.h>
#include "esp_log.h"
// freertos/semphr.h is included via global_vars.h -> freertos/FreeRTOS.h or directly if needed

static const char *TAG_SL = "SENSOR_LOGIC";

// --- Configuration (copied from original main.c, can be centralized if needed) ---
#define SENSOR_UPDATE_INTERVAL_MS 5000
#define EMERGENCY_SIM_INTERVAL_COUNT 3
#define EMERGENCY_DURATION_SENSOR_CYCLES 2

void sensor_simulation_task(void *pvParameters) {
    uint8_t trigger_counter = 0;
    uint8_t emergency_active_duration_counter = 0;
    emergency_type_t next_emergency_to_simulate = EMERGENCY_TYPE_DANGER;

    ESP_LOGI(TAG_SL, "Sensor simulation task started.");
    configure_sensor();  // Forced mode every cycle
    vTaskDelay(pdMS_TO_TICKS(50));
    int32_t t_raw, p_raw, h_raw;
    uint16_t gas_adc;
    uint8_t gas_range;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(SENSOR_UPDATE_INTERVAL_MS));


        read_raw_data(&t_raw, &p_raw, &h_raw, &gas_adc, &gas_range);

        g_temperature = compensate_temperature(t_raw);
        g_pressure = compensate_pressure(p_raw);
        g_humidity = compensate_humidity(h_raw);
        g_gas = compensate_gas(gas_adc, gas_range);

        ESP_LOGI(TAG_SL, "T: %.2f °C | P: %.2f Pa | H: %.2f %% | Gas: %.2f Ohm", g_temperature, g_pressure, g_humidity, g_gas);

        vTaskDelay(pdMS_TO_TICKS(2000));


        if (xSemaphoreTake(g_display_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (emergency_active_duration_counter > 0) {
                emergency_active_duration_counter--;
                if (emergency_active_duration_counter == 0) {
                    ESP_LOGI(TAG_SL, "Clearing simulated emergency.");
                    g_current_emergency_type = EMERGENCY_TYPE_NONE;
                }
            } else {
                trigger_counter++;
                if (trigger_counter >= EMERGENCY_SIM_INTERVAL_COUNT) {
                    trigger_counter = 0;
                    if (g_current_emergency_type == EMERGENCY_TYPE_NONE) {
                        g_current_emergency_type = next_emergency_to_simulate;
                        emergency_active_duration_counter = EMERGENCY_DURATION_SENSOR_CYCLES;
                        ESP_LOGW(TAG_SL, "Simulating EMERGENCY: %s for %d sensor cycles",
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
            ESP_LOGW(TAG_SL, "Sensor task could not acquire mutex for emergency simulation.");
        }
    }
}

