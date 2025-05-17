#ifndef SENSOR_H
#define SENSOR_H

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "sensor.h"

// Constants
#define TAG "BME690"
#define I2C_PORT 0
#define SDA_PIN 21
#define SCL_PIN 22
#define BME690_ADDR 0x76

#define REG_CHIP_ID        0xD0
#define CHIP_ID_VAL        0x61
#define REG_CTRL_HUM       0x72
#define REG_CTRL_MEAS      0x74
#define REG_CONFIG         0x75
#define REG_DATA_START     0x1F

#define OVERSAMPLING_T     0x02  // x2
#define OVERSAMPLING_P     0x05  // x16
#define OVERSAMPLING_H     0x01  // x1
#define IIR_FILTER         0x02  // filter coefficient = 3

// Calibration data structures
typedef struct {
    uint16_t par_t1;
    int16_t par_t2;
    int8_t par_t3;
} bme_temp_calib_data_t;

typedef struct {
    int16_t par_h1;
    int16_t par_h2;
    int8_t  par_h3;
    int8_t  par_h4;
    int8_t  par_h5;
    uint8_t par_h6;
    int8_t  par_h7;
} bme_humidity_calib_data_t;

typedef struct {
    int8_t par_g1;
    int16_t par_g2;
    int8_t par_g3;
    uint8_t res_heat_range;
    int8_t res_heat_val;
    int8_t range_sw_err;
} bme_gas_calib_data_t;

// Function declarations
esp_err_t write_register(uint8_t reg, uint8_t value);
esp_err_t read_registers(uint8_t reg, uint8_t *buf, size_t len);

void configure_sensor(void);
void read_raw_data(int32_t *temp_raw, int32_t *press_raw, int32_t *hum_raw, uint16_t *gas_res_adc, uint8_t *gas_range);

void read_temperature_calibration(void);
void read_humidity_calibration(void);
void read_gas_calibration(void);

float compensate_temperature(int32_t adc_T);
float compensate_pressure(int32_t adc_P);
float compensate_humidity(int32_t adc_H);
float compensate_gas(uint16_t gas_adc, uint8_t gas_range);

#endif // SENSOR_H
