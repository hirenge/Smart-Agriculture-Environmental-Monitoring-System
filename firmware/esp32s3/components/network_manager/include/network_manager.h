#pragma once

#include <stdbool.h>

#include "config_manager.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#define NETWORK_MANAGER_STA_CONNECTED_BIT BIT0
#define NETWORK_MANAGER_AP_STARTED_BIT BIT1

typedef struct {
    EventGroupHandle_t events;
    bool initialized;
    bool sta_enabled;
    bool ap_enabled;
    bool sntp_initialized;
} network_manager_t;

esp_err_t network_manager_init(
    network_manager_t *manager, const saems_config_t *config);
bool network_manager_is_connected(network_manager_t *manager);
bool network_manager_is_ap_active(network_manager_t *manager);
EventBits_t network_manager_wait_for_connection(
    network_manager_t *manager, TickType_t timeout);
