#pragma once

#include <stdbool.h>

#include "esp_err.h"

typedef enum {
    RELAY_CHANNEL_1 = 0,
    RELAY_CHANNEL_2 = 1,
} relay_channel_t;

typedef struct {
    bool active_low;
    bool enabled[2];
} relay_controller_t;

esp_err_t relay_init(relay_controller_t *controller, bool active_low);
esp_err_t relay_set(
    relay_controller_t *controller, relay_channel_t channel, bool enabled);
bool relay_is_enabled(
    const relay_controller_t *controller, relay_channel_t channel);
