#include "sync_manager.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "SYNC_MANAGER";

static SemaphoreHandle_t manager_mutex(sync_manager_t *manager)
{
    return (SemaphoreHandle_t)manager->mutex;
}

esp_err_t sync_manager_init(
    sync_manager_t *manager,
    storage_manager_t *storage,
    mqtt_manager_t *mqtt)
{
    if (manager == NULL || storage == NULL || mqtt == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    manager->storage = storage;
    manager->mqtt = mqtt;
    manager->in_flight_message_id = -1;
    manager->mutex = xSemaphoreCreateMutex();
    if (manager->mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    manager->initialized = true;
    return ESP_OK;
}

void sync_manager_kick(sync_manager_t *manager)
{
    if (manager == NULL || !manager->initialized ||
        !mqtt_manager_is_connected(manager->mqtt)) {
        return;
    }
    xSemaphoreTake(manager_mutex(manager), portMAX_DELAY);
    if (manager->in_flight_message_id >= 0) {
        xSemaphoreGive(manager_mutex(manager));
        return;
    }

    char payload[SAEMS_STORAGE_RECORD_SIZE];
    esp_err_t result = storage_manager_peek(
        manager->storage, payload, sizeof(payload));
    if (result == ESP_ERR_NOT_FOUND) {
        xSemaphoreGive(manager_mutex(manager));
        return;
    }
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Read pending record failed: %s", esp_err_to_name(result));
        xSemaphoreGive(manager_mutex(manager));
        return;
    }

    int message_id = mqtt_manager_publish(manager->mqtt, payload);
    if (message_id >= 0) {
        manager->in_flight_message_id = message_id;
        ESP_LOGI(TAG,
                 "Sync pending record; msg_id=%d remaining=%u",
                 message_id,
                 (unsigned)storage_manager_pending_count(manager->storage));
    }
    xSemaphoreGive(manager_mutex(manager));
}

void sync_manager_on_published(sync_manager_t *manager, int message_id)
{
    if (manager == NULL || !manager->initialized) {
        return;
    }
    xSemaphoreTake(manager_mutex(manager), portMAX_DELAY);
    if (message_id != manager->in_flight_message_id) {
        xSemaphoreGive(manager_mutex(manager));
        return;
    }
    esp_err_t result = storage_manager_pop(manager->storage);
    manager->in_flight_message_id = -1;
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Remove acknowledged record failed: %s", esp_err_to_name(result));
    }
    xSemaphoreGive(manager_mutex(manager));
    sync_manager_kick(manager);
}

void sync_manager_on_connection_changed(
    sync_manager_t *manager, bool connected)
{
    if (manager == NULL || !manager->initialized) {
        return;
    }
    if (!connected) {
        xSemaphoreTake(manager_mutex(manager), portMAX_DELAY);
        manager->in_flight_message_id = -1;
        xSemaphoreGive(manager_mutex(manager));
        return;
    }
    sync_manager_kick(manager);
}
