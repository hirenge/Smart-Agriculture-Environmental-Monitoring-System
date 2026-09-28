#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t relay_init(void);
esp_err_t relay_set(bool enabled);
esp_err_t relay_get_state(bool *enabled);

#ifdef __cplusplus
}
#endif
