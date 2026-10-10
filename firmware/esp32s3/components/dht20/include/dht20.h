#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

typedef struct {
    i2c_master_dev_handle_t device;
} dht20_t;

esp_err_t dht20_init(i2c_master_bus_handle_t bus, dht20_t *sensor);
esp_err_t dht20_read(dht20_t *sensor, float *temperature_c, float *humidity_pct);
