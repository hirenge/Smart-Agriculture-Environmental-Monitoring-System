#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

typedef esp_err_t (*direct_json_provider_t)(
    void *context, char *output, size_t output_size);
typedef esp_err_t (*direct_history_provider_t)(
    void *context, size_t limit, char *output, size_t output_size);
typedef esp_err_t (*direct_json_consumer_t)(
    void *context, const char *json, size_t json_length);

typedef struct {
    direct_json_provider_t device_info;
    direct_json_provider_t status;
    direct_json_provider_t latest_telemetry;
    direct_history_provider_t telemetry_history;
    direct_json_provider_t get_config;
    direct_json_consumer_t set_config;
    direct_json_consumer_t command;
    void *context;
} direct_connection_callbacks_t;

typedef struct {
    void *server;
    direct_connection_callbacks_t callbacks;
    bool running;
} ap_server_t;

typedef struct {
    direct_connection_callbacks_t callbacks;
    uint16_t connection_handle;
    uint16_t telemetry_value_handle;
    bool connected;
    bool subscribed;
    bool initialized;
} ble_manager_t;

esp_err_t ap_server_start(
    ap_server_t *server, const direct_connection_callbacks_t *callbacks);
bool ap_server_is_running(ap_server_t *server);

esp_err_t ble_manager_start(
    ble_manager_t *manager,
    const char *device_name,
    const direct_connection_callbacks_t *callbacks);
esp_err_t ble_manager_notify_telemetry(
    ble_manager_t *manager, const char *payload);
bool ble_manager_is_connected(ble_manager_t *manager);
