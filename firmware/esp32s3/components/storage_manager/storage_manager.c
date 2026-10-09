#include "storage_manager.h"

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"

#define STORAGE_NAMESPACE "saems_store"
#define STORAGE_HEAD_KEY "head"
#define STORAGE_COUNT_KEY "count"

static SemaphoreHandle_t manager_mutex(storage_manager_t *manager)
{
    return (SemaphoreHandle_t)manager->mutex;
}

static void record_key(uint16_t index, char *key, size_t key_size)
{
    (void)snprintf(key, key_size, "r%02u", (unsigned)index);
}

static esp_err_t save_metadata(
    nvs_handle_t handle, const storage_manager_t *manager)
{
    esp_err_t result = nvs_set_u16(handle, STORAGE_HEAD_KEY, manager->head);
    if (result == ESP_OK) {
        result = nvs_set_u16(handle, STORAGE_COUNT_KEY, manager->count);
    }
    return result;
}

esp_err_t storage_manager_init(storage_manager_t *manager)
{
    if (manager == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(manager, 0, sizeof(*manager));
    SemaphoreHandle_t mutex = xSemaphoreCreateMutex();
    if (mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    manager->mutex = mutex;

    nvs_handle_t handle;
    esp_err_t result = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle);
    if (result != ESP_OK) {
        return result;
    }
    uint16_t head = 0;
    uint16_t count = 0;
    if (nvs_get_u16(handle, STORAGE_HEAD_KEY, &head) != ESP_OK) {
        head = 0;
    }
    if (nvs_get_u16(handle, STORAGE_COUNT_KEY, &count) != ESP_OK) {
        count = 0;
    }
    nvs_close(handle);

    manager->head = head % SAEMS_STORAGE_MAX_RECORDS;
    manager->count = count > SAEMS_STORAGE_MAX_RECORDS
                         ? SAEMS_STORAGE_MAX_RECORDS
                         : count;
    manager->initialized = true;
    return ESP_OK;
}

esp_err_t storage_manager_save(
    storage_manager_t *manager, const char *payload)
{
    if (manager == NULL || payload == NULL || !manager->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    size_t payload_length = strlen(payload);
    if (payload_length == 0 || payload_length >= SAEMS_STORAGE_RECORD_SIZE) {
        return ESP_ERR_INVALID_SIZE;
    }

    xSemaphoreTake(manager_mutex(manager), portMAX_DELAY);
    nvs_handle_t handle;
    esp_err_t result = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle);
    if (result != ESP_OK) {
        xSemaphoreGive(manager_mutex(manager));
        return result;
    }

    if (manager->count == SAEMS_STORAGE_MAX_RECORDS) {
        /* Do not evict the head while Sync Manager may be waiting for its
         * PUBACK: that acknowledgement would otherwise remove the next,
         * unsent record. Preserve pending records and reject the new sample. */
        nvs_close(handle);
        xSemaphoreGive(manager_mutex(manager));
        return ESP_ERR_NO_MEM;
    }

    uint16_t tail = (manager->head + manager->count) %
                    SAEMS_STORAGE_MAX_RECORDS;
    char key[8];
    record_key(tail, key, sizeof(key));
    result = nvs_set_str(handle, key, payload);
    if (result == ESP_OK) {
        manager->count++;
        result = save_metadata(handle, manager);
    }
    if (result == ESP_OK) {
        result = nvs_commit(handle);
    }
    nvs_close(handle);
    xSemaphoreGive(manager_mutex(manager));
    return result;
}

esp_err_t storage_manager_peek(
    storage_manager_t *manager, char *payload, size_t payload_size)
{
    if (manager == NULL || payload == NULL || payload_size == 0 ||
        !manager->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTake(manager_mutex(manager), portMAX_DELAY);
    if (manager->count == 0) {
        xSemaphoreGive(manager_mutex(manager));
        return ESP_ERR_NOT_FOUND;
    }

    nvs_handle_t handle;
    esp_err_t result = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &handle);
    if (result == ESP_OK) {
        char key[8];
        record_key(manager->head, key, sizeof(key));
        result = nvs_get_str(handle, key, payload, &payload_size);
        nvs_close(handle);
    }
    xSemaphoreGive(manager_mutex(manager));
    return result;
}

esp_err_t storage_manager_pop(storage_manager_t *manager)
{
    if (manager == NULL || !manager->initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTake(manager_mutex(manager), portMAX_DELAY);
    if (manager->count == 0) {
        xSemaphoreGive(manager_mutex(manager));
        return ESP_ERR_NOT_FOUND;
    }

    nvs_handle_t handle;
    esp_err_t result = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &handle);
    if (result == ESP_OK) {
        char key[8];
        record_key(manager->head, key, sizeof(key));
        result = nvs_erase_key(handle, key);
        if (result == ESP_OK || result == ESP_ERR_NVS_NOT_FOUND) {
            manager->head =
                (manager->head + 1U) % SAEMS_STORAGE_MAX_RECORDS;
            manager->count--;
            result = save_metadata(handle, manager);
        }
        if (result == ESP_OK) {
            result = nvs_commit(handle);
        }
        nvs_close(handle);
    }
    xSemaphoreGive(manager_mutex(manager));
    return result;
}

size_t storage_manager_pending_count(storage_manager_t *manager)
{
    if (manager == NULL || !manager->initialized) {
        return 0;
    }
    xSemaphoreTake(manager_mutex(manager), portMAX_DELAY);
    size_t count = manager->count;
    xSemaphoreGive(manager_mutex(manager));
    return count;
}

esp_err_t storage_manager_history_json(
    storage_manager_t *manager,
    size_t limit,
    char *output,
    size_t output_size)
{
    if (manager == NULL || output == NULL || output_size < 16 ||
        !manager->initialized) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(manager_mutex(manager), portMAX_DELAY);
    size_t count = manager->count;
    if (limit < count) {
        count = limit;
    }
    size_t used = (size_t)snprintf(output, output_size, "{\"items\":[");
    nvs_handle_t handle;
    esp_err_t result = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &handle);
    if (result != ESP_OK) {
        xSemaphoreGive(manager_mutex(manager));
        return result;
    }

    for (size_t offset = 0; offset < count; offset++) {
        uint16_t index = (manager->head + offset) %
                         SAEMS_STORAGE_MAX_RECORDS;
        char key[8];
        char record[SAEMS_STORAGE_RECORD_SIZE];
        size_t record_size = sizeof(record);
        record_key(index, key, sizeof(key));
        result = nvs_get_str(handle, key, record, &record_size);
        if (result != ESP_OK) {
            break;
        }
        int written = snprintf(output + used,
                               output_size - used,
                               "%s%s",
                               offset == 0 ? "" : ",",
                               record);
        if (written < 0 || (size_t)written >= output_size - used) {
            result = ESP_ERR_INVALID_SIZE;
            break;
        }
        used += (size_t)written;
    }
    nvs_close(handle);

    if (result == ESP_OK) {
        int written = snprintf(output + used,
                               output_size - used,
                               "],\"pending_count\":%u}",
                               (unsigned)manager->count);
        if (written < 0 || (size_t)written >= output_size - used) {
            result = ESP_ERR_INVALID_SIZE;
        }
    }
    if (result != ESP_OK) {
        output[0] = '\0';
    }
    xSemaphoreGive(manager_mutex(manager));
    return result;
}
