#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "sensor.h"

extern i2c_master_bus_handle_t bus;
extern i2c_master_dev_handle_t dev;
//static int32_t t_fine;  


void app_main(void) {
    // I2C Setup
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
        ESP_LOGE(TAG, "Unexpected chip ID: 0x%02X", id);
        return;
    }

    read_temperature_calibration();
    read_humidity_calibration();
    read_gas_calibration();

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

        ESP_LOGI(TAG, "T: %.2f °C | P: %.2f Pa | H: %.2f %% | Gas: %.2f Ohm", temp, press, hum, gas);

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
