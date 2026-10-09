#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#define SAEMS_STORAGE_MAX_RECORDS 32
#define SAEMS_STORAGE_RECORD_SIZE 768

typedef struct {
    uint16_t head;
    uint16_t count;
    void *mutex;
    bool initialized;
} storage_manager_t;

esp_err_t storage_manager_init(storage_manager_t *manager);
esp_err_t storage_manager_save(
    storage_manager_t *manager, const char *payload);
esp_err_t storage_manager_peek(
    storage_manager_t *manager, char *payload, size_t payload_size);
esp_err_t storage_manager_pop(storage_manager_t *manager);
size_t storage_manager_pending_count(storage_manager_t *manager);
esp_err_t storage_manager_history_json(
    storage_manager_t *manager,
    size_t limit,
    char *output,
    size_t output_size);
