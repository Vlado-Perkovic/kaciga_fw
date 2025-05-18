#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
// #include "esp_log.h"
#include "ssd1306.h"        // From SSD1306_Driver component
#include "fonts.h"          // From SSD1306_Driver component
#include <math.h>
#include "driver/i2c_master.h"
#include "sensor.h"
#include <math.h>

// Include new local headers
#include "common_types.h"
#include "global_vars.h"    // For extern declarations (optional here as they are defined below)
#include "display_logic.h"
#include "sensor_logic.h"
#include "espnow_logic.h"   // New ESP-NOW logic

#define TAGERICA "APP_MAIN"

// --- Define Global variables ---
// These are now declared 'extern' in global_vars.h for other files to see
volatile float g_temperature = 0.0;
volatile float g_pressure = 0.0;
volatile float g_humidity = 0.0;
volatile float g_gas = 0.0;
volatile emergency_type_t g_current_emergency_type = EMERGENCY_TYPE_NONE;

SemaphoreHandle_t g_display_mutex; // Mutex to protect shared display resources and emergency state
extern i2c_master_bus_handle_t bus;
extern i2c_master_dev_handle_t dev;


void app_main() {
    // Initialize the SSD1306 display
    // SSD1306_Init() handles I2C setup based on defines in ssd1306.h and ssd1306.c
    if (SSD1306_Init()) {
        ESP_LOGI(TAGERICA, "SSD1306 Initialized Successfully.");
    } else {
        ESP_LOGE(TAGERICA, "SSD1306 Initialization Failed!");
        // Handle initialization failure (e.g., halt or retry)
        return;
    }
    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_PORT,
        .sda_io_num = SDA_PIN,
        .scl_io_num = SCL_PIN,
        .glitch_ignore_cnt = 7,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &bus));

    i2c_device_config_t dev_cfg = {
        .device_address = BME690_ADDR,
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .scl_speed_hz = 100000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus, &dev_cfg, &dev));

    uint8_t id = 0;
    read_registers(REG_CHIP_ID, &id, 1);
    if (id != CHIP_ID_VAL) {
        ESP_LOGE(TAGERICA, "Unexpected chip ID: 0x%02X", id);
        return;
    }

    read_temperature_calibration();
    read_humidity_calibration();
    read_gas_calibration();

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
        ESP_LOGE(TAGERICA, "Failed to create display mutex!");
        return; // Critical error
    }
// Initialize ESP-NOW
    // Initialize ESP-NOW and start its task
    if (app_espnow_init_and_start_task() != ESP_OK) {
        ESP_LOGE(TAGERICA, "ESP-NOW Initialization and Task Start Failed!");
        // Handle failure as needed
    } else {
        ESP_LOGI(TAGERICA, "ESP-NOW Initialized and Task Started.");
    }
    // Create the display task (function is now in display_logic.c)
    if (xTaskCreate(&display_task, "display_oled_task", 2048 * 2, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAGERICA, "Failed to create display_task!");
        if (g_display_mutex != NULL) vSemaphoreDelete(g_display_mutex); // Clean up mutex
        return; // Critical error
    }
    ESP_LOGI(TAGERICA, "Display task created.");

    // Create the sensor simulation task (function is now in sensor_logic.c)
    // In a real application, this would be your BME680 reading task
    if (xTaskCreate(&sensor_simulation_task, "sensor_sim_task", 2048 * 2, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAGERICA, "Failed to create sensor_simulation_task!");
        // Consider cleanup if display_task was already created and needs to be stopped
        return; // Critical error
    }
    ESP_LOGI(TAGERICA, "Sensor simulation task created.");
    

    while (true) {
        configure_sensor();  // Forced mode every cycle
        vTaskDelay(pdMS_TO_TICKS(50));

        int32_t t_raw, p_raw, h_raw;
        uint16_t gas_adc;
        uint8_t gas_range;
        read_raw_data(&t_raw, &p_raw, &h_raw, &gas_adc, &gas_range);

        float temp = compensate_temperature(t_raw);
        float press = compensate_pressure(p_raw);
        float hum = compensate_humidity(h_raw);
        float gas = compensate_gas(gas_adc, gas_range);

        //ESP_LOGI(TAGERICA, "T: %.2f °C | P: %.2f Pa | H: %.2f %% | Gas: %.2f Ohm", temp, press, hum, gas);

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    ESP_LOGI(TAGERICA, "app_main finished setup. Tasks are running.");
    // app_main can exit now (or enter a low-power mode, or a simple loop if needed for other top-level logic).
    // The FreeRTOS scheduler will continue running the created tasks.
}
