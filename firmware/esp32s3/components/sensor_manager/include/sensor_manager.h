#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "bh1750.h"
#include "dht20.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "soil_moisture.h"

typedef enum {
    SENSOR_HEALTH_UNAVAILABLE = 0,
    SENSOR_HEALTH_OK,
    SENSOR_HEALTH_ERROR,
} sensor_health_t;

typedef struct {
    i2c_port_num_t i2c_port;
    gpio_num_t i2c_sda;
    gpio_num_t i2c_scl;
    uint8_t bh1750_address;
    bool soil_enabled;
    int soil_dry_raw;
    int soil_wet_raw;
} sensor_manager_config_t;

typedef struct {
    float temperature_c;
    float humidity_pct;
    float light_lux;
    float soil_moisture_pct;
    int soil_raw;
    bool soil_digital;

    bool temperature_valid;
    bool humidity_valid;
    bool light_valid;
    bool soil_moisture_valid;

    sensor_health_t dht20_health;
    sensor_health_t bh1750_health;
    sensor_health_t soil_health;
} sensor_snapshot_t;

typedef struct {
    i2c_master_bus_handle_t i2c_bus;
    dht20_t dht20;
    bh1750_t bh1750;
    soil_moisture_t soil;
    bool initialized;
    bool dht20_ready;
    bool bh1750_ready;
    bool soil_enabled;
    bool soil_ready;
} sensor_manager_t;

esp_err_t sensor_manager_init(
    sensor_manager_t *manager, const sensor_manager_config_t *config);
esp_err_t sensor_manager_read_all(
    sensor_manager_t *manager, sensor_snapshot_t *snapshot);
const char *sensor_health_to_string(sensor_health_t health);
