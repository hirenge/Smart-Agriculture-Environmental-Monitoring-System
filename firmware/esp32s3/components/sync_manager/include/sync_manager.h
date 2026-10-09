#pragma once

#include <stdbool.h>

#include "esp_err.h"
#include "mqtt_manager.h"
#include "storage_manager.h"

typedef struct {
    storage_manager_t *storage;
    mqtt_manager_t *mqtt;
    void *mutex;
    int in_flight_message_id;
    bool initialized;
} sync_manager_t;

esp_err_t sync_manager_init(
    sync_manager_t *manager,
    storage_manager_t *storage,
    mqtt_manager_t *mqtt);
void sync_manager_kick(sync_manager_t *manager);
void sync_manager_on_published(sync_manager_t *manager, int message_id);
void sync_manager_on_connection_changed(
    sync_manager_t *manager, bool connected);
