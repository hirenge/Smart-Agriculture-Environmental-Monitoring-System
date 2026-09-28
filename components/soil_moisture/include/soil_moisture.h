#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int raw_adc;
    float voltage_mv;
    float moisture_percent;
} soil_moisture_data_t;

esp_err_t soil_moisture_init(void);
esp_err_t soil_moisture_read(soil_moisture_data_t *data);

#ifdef __cplusplus
}
#endif
