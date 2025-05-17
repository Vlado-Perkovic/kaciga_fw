#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "ssd1306.h"        // From SSD1306_Driver component
#include "fonts.h"          // From SSD1306_Driver component

// Include new local headers
#include "common_types.h"
#include "global_vars.h"    // For extern declarations (optional here as they are defined below)
#include "display_logic.h"
#include "sensor_logic.h"

#define TAG "APP_MAIN"

// --- Define Global variables ---
// These are now declared 'extern' in global_vars.h for other files to see
volatile float g_temperature = 25.0;
volatile float g_pressure = 1012.5;
volatile float g_humidity = 60.0;
volatile emergency_type_t g_current_emergency_type = EMERGENCY_TYPE_NONE;
SemaphoreHandle_t g_display_mutex; // Mutex to protect shared display resources and emergency state


void app_main() {
    // Initialize the SSD1306 display
    // SSD1306_Init() handles I2C setup based on defines in ssd1306.h and ssd1306.c
    if (SSD1306_Init()) {
        ESP_LOGI(TAG, "SSD1306 Initialized Successfully.");
    } else {
        ESP_LOGE(TAG, "SSD1306 Initialization Failed!");
        // Handle initialization failure (e.g., halt or retry)
        return;
    }

    // Initial display message
    SSD1306_Fill(SSD1306_COLOR_BLACK); // Clear buffer before first write
    const char* boot_msg = "Booting...";
    // Calculate position to center the boot message using Font_11x18
    uint16_t boot_msg_width = strlen(boot_msg) * Font_11x18.FontWidth;
    uint16_t boot_msg_x = (SSD1306_WIDTH > boot_msg_width) ? (SSD1306_WIDTH - boot_msg_width) / 2 : 0;
    uint16_t boot_msg_y = (SSD1306_HEIGHT > Font_11x18.FontHeight) ? (SSD1306_HEIGHT - Font_11x18.FontHeight) / 2 : 0;
    SSD1306_GotoXY(boot_msg_x, boot_msg_y);
    SSD1306_Puts(boot_msg, &Font_11x18, SSD1306_COLOR_WHITE);
    SSD1306_UpdateScreen();
    vTaskDelay(pdMS_TO_TICKS(2000)); // Show initial message for a bit

    // Create mutex for display and emergency state
    g_display_mutex = xSemaphoreCreateMutex();
    if (g_display_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create display mutex!");
        return; // Critical error
    }

    // Create the display task (function is now in display_logic.c)
    if (xTaskCreate(&display_task, "display_oled_task", 2048 * 2, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create display_task!");
        if (g_display_mutex != NULL) vSemaphoreDelete(g_display_mutex); // Clean up mutex
        return; // Critical error
    }
    ESP_LOGI(TAG, "Display task created.");

    // Create the sensor simulation task (function is now in sensor_logic.c)
    // In a real application, this would be your BME680 reading task
    if (xTaskCreate(&sensor_simulation_task, "sensor_sim_task", 2048 * 2, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create sensor_simulation_task!");
        // Consider cleanup if display_task was already created and needs to be stopped
        return; // Critical error
    }
    ESP_LOGI(TAG, "Sensor simulation task created.");

    ESP_LOGI(TAG, "app_main finished setup. Tasks are running.");
    // app_main can exit now (or enter a low-power mode, or a simple loop if needed for other top-level logic).
    // The FreeRTOS scheduler will continue running the created tasks.
}
