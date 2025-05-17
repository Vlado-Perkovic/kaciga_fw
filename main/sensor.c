#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "sensor.h"

i2c_master_bus_handle_t bus;
i2c_master_dev_handle_t dev;
int32_t t_fine;

bme_temp_calib_data_t calib;
bme_humidity_calib_data_t hum_calib;
bme_gas_calib_data_t gas_calib;


esp_err_t write_register(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    return i2c_master_transmit(dev, buf, 2, pdMS_TO_TICKS(100));
}

esp_err_t read_registers(uint8_t reg, uint8_t *buf, size_t len) {
    return i2c_master_transmit_receive(dev, &reg, 1, buf, len, pdMS_TO_TICKS(100));
}

void configure_sensor(void) {
    write_register(REG_CTRL_HUM, OVERSAMPLING_H);
    write_register(REG_CONFIG, (IIR_FILTER << 2));  // 0b00001000
    write_register(REG_CTRL_MEAS, (OVERSAMPLING_T << 5) | (OVERSAMPLING_P << 2) | 0x01); // Forced mode
}

void read_raw_data(int32_t *temp_raw, int32_t *press_raw, int32_t *hum_raw, uint16_t *gas_res_adc, uint8_t *gas_range) {
    uint8_t data[15];
    read_registers(REG_DATA_START, data, 15);

    *press_raw = ((uint32_t)data[0] << 12) | ((uint32_t)data[1] << 4) | (data[2] >> 4);
    *temp_raw  = ((uint32_t)data[3] << 12) | ((uint32_t)data[4] << 4) | (data[5] >> 4);
    *hum_raw   = ((uint32_t)data[6] << 8) | data[7];

    *gas_res_adc = ((uint16_t)data[13] << 2) | ((data[14] & 0xC0) >> 6);
    *gas_range   = data[14] & 0x0F;
}


 void read_temperature_calibration(void) {
    uint8_t buf_t1[2], buf_t2[2], buf_t3[1];

    read_registers(0xE9, buf_t1, 2);  // par_t1
    read_registers(0x8A, buf_t2, 2);  // par_t2
    read_registers(0x8C, buf_t3, 1);  // par_t3

    calib.par_t1 = (uint16_t)((buf_t1[1] << 8) | buf_t1[0]);
    calib.par_t2 = (int16_t)((buf_t2[1] << 8) | buf_t2[0]);
    calib.par_t3 = (int8_t)buf_t3[0];

    ESP_LOGI(TAG, "par_t1=%u, par_t2=%d, par_t3=%d", calib.par_t1, calib.par_t2, calib.par_t3);
}

 void read_humidity_calibration(void) {
    uint8_t buf[7];
    read_registers(0xE1, buf, 7);

    // Extracting all 7 humidity calibration parameters
    hum_calib.par_h1 = (int16_t)((buf[0] << 4) | (buf[1] & 0x0F));
    hum_calib.par_h2 = (int16_t)((buf[2] << 4) | (buf[1] >> 4));

    hum_calib.par_h3 = (int8_t)buf[3];

    hum_calib.par_h4 = (int8_t)((int8_t)buf[4] << 4 | (buf[5] & 0x0F));
    hum_calib.par_h5 = (int8_t)((int8_t)buf[5] >> 4 | (buf[6] << 4));

    read_registers(0xE9, &hum_calib.par_h6, 1); // unsigned
    read_registers(0xEA, (uint8_t*)&hum_calib.par_h7, 1); // signed

    ESP_LOGI(TAG, "Humidity calibration:");
    ESP_LOGI(TAG, "par_h1: %d", hum_calib.par_h1);
    ESP_LOGI(TAG, "par_h2: %d", hum_calib.par_h2);
    ESP_LOGI(TAG, "par_h3: %d", hum_calib.par_h3);
    ESP_LOGI(TAG, "par_h4: %d", hum_calib.par_h4);
    ESP_LOGI(TAG, "par_h5: %d", hum_calib.par_h5);
    ESP_LOGI(TAG, "par_h6: %u", hum_calib.par_h6);
    ESP_LOGI(TAG, "par_h7: %d", hum_calib.par_h7);
}

 void read_gas_calibration(void) {
    uint8_t buf_g1, buf_g2[2], buf_g3;
    uint8_t res_heat_val, res_heat_range, range_err;

    read_registers(0xED, &buf_g1, 1);
    read_registers(0xEC, buf_g2, 2); // LSB first for par_g2
    read_registers(0xEE, &buf_g3, 1);
    read_registers(0x00, &res_heat_val, 1); // res_heat_val
    read_registers(0x02, &res_heat_range, 1); // bits [6:4]
    read_registers(0x04, &range_err, 1); // bits [6:4]

    gas_calib.par_g1 = (int8_t)buf_g1;
    gas_calib.par_g2 = (int16_t)((buf_g2[1] << 8) | buf_g2[0]);
    gas_calib.par_g3 = (int8_t)buf_g3;
    gas_calib.res_heat_val = (int8_t)res_heat_val;
    gas_calib.res_heat_range = (res_heat_range & 0x30) >> 4;
    gas_calib.range_sw_err = (int8_t)((range_err & 0xF0) >> 4);

    ESP_LOGI(TAG, "Gas Calibration: g1=%d g2=%d g3=%d rhr=%d rhv=%d rse=%d",
             gas_calib.par_g1, gas_calib.par_g2, gas_calib.par_g3,
             gas_calib.res_heat_range, gas_calib.res_heat_val, gas_calib.range_sw_err);
}

 float compensate_temperature(int32_t adc_T) {

    ESP_LOGI(TAG, "Raw temp: %ld", adc_T);
    float var1 = (((float)adc_T) / 16384.0f - ((float)calib.par_t1) / 1024.0f) * ((float)calib.par_t2);
    float var2 = ((((float)adc_T) / 131072.0f - ((float)calib.par_t1) / 8192.0f) *
                  (((float)adc_T) / 131072.0f - ((float)calib.par_t1) / 8192.0f)) * ((float)calib.par_t3);
    t_fine = (int32_t)(var1 + var2);
    return (var1 + var2) / 5120.0f;
}


 float compensate_pressure(int32_t adc_P) {
    // Dummy calibration values (replace with real ones after reading from sensor)
    int64_t var1, var2, p;
    int64_t par_p1 = 36477, par_p2 = -10685, par_p3 = 3024;
    int64_t par_p4 = 2855, par_p5 = 140, par_p6 = -7;
    int64_t par_p7 = 15500, par_p8 = -14600, par_p9 = 6000;

    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * par_p6;
    var2 = var2 + ((var1 * par_p5) << 17);
    var2 = var2 + ((int64_t)par_p4 << 35);
    var1 = ((var1 * par_p3) >> 8) + ((par_p2 * var1) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * par_p1 >> 33;

    if (var1 == 0) return 0;

    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (par_p9 * (p >> 13) * (p >> 13)) >> 25;
    var2 = (par_p8 * p) >> 19;

    p = ((p + var1 + var2) >> 8) + ((int64_t)par_p7 << 4);
    return (float)p / 256.0f;
}

 float compensate_humidity(int32_t adc_H) {
    int32_t v_x1;

    int32_t temp_scaled = ((t_fine * 5) + 128) >> 8;  // temperature in 0.01°C units

    int32_t var1 = adc_H - ((hum_calib.par_h1 * 16)) - (((temp_scaled * hum_calib.par_h3) / 100) >> 1);
    int32_t var2 = (hum_calib.par_h2 * (((temp_scaled * hum_calib.par_h4) / 100) +
                   (((temp_scaled * ((temp_scaled * hum_calib.par_h5) / 100)) >> 6) / 100) + (1 << 14))) >> 10;
    int32_t var3 = var1 * var2;
    int32_t var4 = ((hum_calib.par_h6 << 7) + ((temp_scaled * hum_calib.par_h7) / 100)) >> 4;
    int32_t var5 = ((var3 >> 14) * (var3 >> 14)) >> 10;
    int32_t var6 = (var4 * var5) >> 1;

    int32_t hum_comp = ((var3 + var6) >> 10);
    hum_comp = (hum_comp > 100000 ? 100000 : (hum_comp < 0 ? 0 : hum_comp));

    return hum_comp / 1000.0f;  // %RH
}

 float compensate_gas(uint16_t gas_adc, uint8_t gas_range) {
     const uint32_t lookupTable1[16] = {
        2147483647, 2147483647, 2147483647, 2147483647,
        2147483647, 2126008810, 2147483647, 2130303777,
        2147483647, 2147483647, 2143188679, 2136746228,
        2147483647, 2126008810, 2147483647, 2147483647
    };
     const uint32_t lookupTable2[16] = {
        4096000000, 2048000000, 1024000000, 512000000,
        255744255, 127110228, 64000000, 32258064,
        16016016, 8000000, 4000000, 2000000,
        1000000, 500000, 250000, 125000
    };

    float var1 = (1340.0f + 5.0f * gas_calib.range_sw_err);
    float var2 = (lookupTable1[gas_range] / 65536.0f);
    float var3 = 1.0f + (gas_adc / 262144.0f);
    float gas_res = var2 * var1 * var3;

    return gas_res;  // in Ohms
}
