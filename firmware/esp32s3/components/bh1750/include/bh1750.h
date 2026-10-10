#pragma once

#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

typedef struct {
    i2c_master_dev_handle_t device;
} bh1750_t;

esp_err_t bh1750_init(
    i2c_master_bus_handle_t bus, uint8_t address, bh1750_t *sensor);
esp_err_t bh1750_read_lux(bh1750_t *sensor, float *light_lux);
