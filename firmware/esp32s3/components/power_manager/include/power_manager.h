#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef enum {
    POWER_MANAGER_ACTIVE = 0,
    POWER_MANAGER_MODEM_SAVE,
    POWER_MANAGER_LIGHT_SLEEP,
    POWER_MANAGER_DEEP_SLEEP,
} power_manager_mode_t;

typedef struct {
    power_manager_mode_t mode;
    bool initialized;
} power_manager_t;

esp_err_t power_manager_init(
    power_manager_t *manager, power_manager_mode_t mode);
esp_err_t power_manager_idle(
    power_manager_t *manager,
    uint32_t interval_seconds,
    bool direct_connection_active,
    bool sync_pending);
