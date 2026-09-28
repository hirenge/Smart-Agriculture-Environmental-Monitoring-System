#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float temperature_c;
    float humidity_percent;
} dht20_data_t;

esp_err_t dht20_init(void);
esp_err_t dht20_read(dht20_data_t *data);

#ifdef __cplusplus
}
#endif
