#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "config_manager.h"
#include "esp_err.h"
#include "mqtt_client.h"

typedef void (*mqtt_manager_command_callback_t)(
    const char *payload, size_t payload_length, void *context);
typedef void (*mqtt_manager_published_callback_t)(
    int message_id, void *context);
typedef void (*mqtt_manager_connection_callback_t)(
    bool connected, void *context);

typedef struct {
    esp_mqtt_client_handle_t client;
    char telemetry_topic[96];
    char command_topic[96];
    mqtt_manager_command_callback_t command_callback;
    mqtt_manager_published_callback_t published_callback;
    mqtt_manager_connection_callback_t connection_callback;
    void *callback_context;
    bool connected;
    bool initialized;
} mqtt_manager_t;

esp_err_t mqtt_manager_init(
    mqtt_manager_t *manager,
    const saems_config_t *config,
    mqtt_manager_command_callback_t command_callback,
    mqtt_manager_published_callback_t published_callback,
    mqtt_manager_connection_callback_t connection_callback,
    void *callback_context);
bool mqtt_manager_is_connected(mqtt_manager_t *manager);
int mqtt_manager_publish(mqtt_manager_t *manager, const char *payload);
