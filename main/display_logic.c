#include "display_logic.h" // Includes global_vars.h, common_types.h, ssd1306.h, fonts.h
#include <stdio.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "DISPLAY_LOGIC";

// --- Configuration (copied from original main.c, can be centralized if needed) ---
#define DISPLAY_UPDATE_INTERVAL_MS 250 // Update display every 0.25 seconds for more rapid blink

void display_task(void *pvParameters) {
    char temp_str[20];
    char pressure_str[20];
    char humidity_str[20];
    static bool s_emergency_blink_visible = true; // For blinking effect

    ESP_LOGI(TAG, "Display task started.");

    while (1) {
        if (xSemaphoreTake(g_display_mutex, portMAX_DELAY) == pdTRUE) {
            SSD1306_Fill(SSD1306_COLOR_BLACK); // Clear display buffer

            emergency_type_t current_emergency = g_current_emergency_type; // Copy volatile to local

            if (current_emergency != EMERGENCY_TYPE_NONE) {
                const char* emergency_text = "";
                if (current_emergency == EMERGENCY_TYPE_DANGER) {
                    emergency_text = "DANGER";
                } else if (current_emergency == EMERGENCY_TYPE_FALL) {
                    emergency_text = "FALL";
                }

                if (s_emergency_blink_visible) {
                    uint16_t text_width = strlen(emergency_text) * Font_16x26.FontWidth;
                    uint16_t x_pos = (SSD1306_WIDTH - text_width) / 2;
                    uint16_t y_pos = (SSD1306_HEIGHT - Font_16x26.FontHeight) / 2;
                    if (x_pos > SSD1306_WIDTH) x_pos = 0; 

                    SSD1306_GotoXY(x_pos, y_pos);
                    SSD1306_Puts((char*)emergency_text, &Font_16x26, SSD1306_COLOR_WHITE);
                }
                s_emergency_blink_visible = !s_emergency_blink_visible; // Toggle blink state
            } else {
                // No emergency, display sensor data
                snprintf(temp_str, sizeof(temp_str), "Temp: %.1f C", g_temperature);
                snprintf(pressure_str, sizeof(pressure_str), "Pres: %.1f hPa", g_pressure);
                snprintf(humidity_str, sizeof(humidity_str), "Humi: %.0f %%", g_humidity);

                SSD1306_GotoXY(0, 0);
                SSD1306_Puts(temp_str, &Font_11x18, SSD1306_COLOR_WHITE);

                SSD1306_GotoXY(0, Font_11x18.FontHeight + 4);
                SSD1306_Puts(pressure_str, &Font_11x18, SSD1306_COLOR_WHITE);

                SSD1306_GotoXY(0, (Font_11x18.FontHeight + 4) * 2);
                SSD1306_Puts(humidity_str, &Font_11x18, SSD1306_COLOR_WHITE);

                s_emergency_blink_visible = true; // Reset blink state for next potential emergency
            }

            SSD1306_UpdateScreen();
            xSemaphoreGive(g_display_mutex);

        }

        vTaskDelay(pdMS_TO_TICKS(DISPLAY_UPDATE_INTERVAL_MS));
    }
}

