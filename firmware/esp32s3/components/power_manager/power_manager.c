#include "power_manager.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "POWER_MANAGER";

esp_err_t power_manager_init(
    power_manager_t *manager, power_manager_mode_t mode)
{
    if (manager == NULL || mode > POWER_MANAGER_DEEP_SLEEP) {
        return ESP_ERR_INVALID_ARG;
    }
    manager->mode = mode;
    manager->initialized = true;
    if (mode == POWER_MANAGER_MODEM_SAVE) {
        return esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    }
    return ESP_OK;
}

esp_err_t power_manager_idle(
    power_manager_t *manager,
    uint32_t interval_seconds,
    bool direct_connection_active,
    bool sync_pending)
{
    if (manager == NULL || !manager->initialized || interval_seconds == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (manager->mode == POWER_MANAGER_LIGHT_SLEEP &&
        !direct_connection_active && !sync_pending) {
        ESP_RETURN_ON_ERROR(
            esp_sleep_enable_timer_wakeup(
                (uint64_t)interval_seconds * 1000000ULL),
            TAG,
            "light sleep timer");
        ESP_LOGI(TAG, "Entering light sleep for %u seconds",
                 (unsigned)interval_seconds);
        return esp_light_sleep_start();
    }

    if (manager->mode == POWER_MANAGER_DEEP_SLEEP &&
        !direct_connection_active && !sync_pending) {
        ESP_RETURN_ON_ERROR(
            esp_sleep_enable_timer_wakeup(
                (uint64_t)interval_seconds * 1000000ULL),
            TAG,
            "deep sleep timer");
        ESP_LOGW(TAG, "Entering deep sleep for %u seconds",
                 (unsigned)interval_seconds);
        esp_deep_sleep_start();
    }

    vTaskDelay(pdMS_TO_TICKS(interval_seconds * 1000U));
    return ESP_OK;
}
