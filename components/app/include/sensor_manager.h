#pragma once

#include "esp_err.h"
#include "dht20.h"
#include "soil_moisture.h"
#include "light_sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    dht20_data_t dht20;
    soil_moisture_data_t soil;
    light_sensor_data_t light;
    bool pump_state;
} sensor_snapshot_t;

esp_err_t sensor_manager_init(void);
esp_err_t sensor_manager_sample(sensor_snapshot_t *snapshot);

#ifdef __cplusplus
}
#endif
