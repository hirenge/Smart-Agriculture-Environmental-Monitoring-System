#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float lux;
    uint16_t raw;
} light_sensor_data_t;

esp_err_t light_sensor_init(void);
esp_err_t light_sensor_read(light_sensor_data_t *data);

#ifdef __cplusplus
}
#endif
