#pragma once

#include <stdbool.h>

#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"

typedef struct {
    adc_oneshot_unit_handle_t adc;
    int dry_raw;
    int wet_raw;
} soil_moisture_t;

esp_err_t soil_moisture_init(
    soil_moisture_t *sensor, int dry_raw, int wet_raw);
esp_err_t soil_moisture_read(
    soil_moisture_t *sensor,
    float *moisture_pct,
    int *raw_value,
    bool *digital_level);
