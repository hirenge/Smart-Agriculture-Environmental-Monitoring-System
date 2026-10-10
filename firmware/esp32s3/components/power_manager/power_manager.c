#include "power_manager.h"

#include "esp_check.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

static const char *TAG = "POWER_MANAGER";

const char *power_manager_mode_name(power_manager_mode_t mode)
{
    switch (mode) {
    case POWER_MANAGER_ACTIVE:
        return "active";
    case POWER_MANAGER_MODEM_SAVE:
        return "modem-save";
    case POWER_MANAGER_LIGHT_SLEEP:
        return "light-sleep";
    case POWER_MANAGER_DEEP_SLEEP:
        return "deep-sleep";
    default:
        return "invalid";
    }
}

bool power_manager_is_deep_sleep(const power_manager_t *manager)
{
    return manager != NULL && manager->initialized &&
           manager->mode == POWER_MANAGER_DEEP_SLEEP;
}

esp_err_t power_manager_init(
    power_manager_t *manager, power_manager_mode_t mode)
{
    if (manager == NULL || mode > POWER_MANAGER_DEEP_SLEEP) {
        return ESP_ERR_INVALID_ARG;
    }
    manager->initialized = false;
    return power_manager_set_mode(manager, mode);
}

esp_err_t power_manager_set_mode(
    power_manager_t *manager, power_manager_mode_t mode)
{
    if (manager == NULL || mode > POWER_MANAGER_DEEP_SLEEP) {
        return ESP_ERR_INVALID_ARG;
    }
    if (manager->initialized && manager->mode == mode) {
        return ESP_OK;
    }

#ifdef CONFIG_PM_ENABLE
    const bool automatic_light_sleep = mode == POWER_MANAGER_LIGHT_SLEEP;
    const esp_pm_config_t config = {
        .max_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        .min_freq_mhz = automatic_light_sleep
                            ? CONFIG_XTAL_FREQ
                            : CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
        .light_sleep_enable = automatic_light_sleep,
    };
    ESP_RETURN_ON_ERROR(
        esp_pm_configure(&config), TAG, "configure runtime power mode");
#else
    if (mode == POWER_MANAGER_LIGHT_SLEEP) {
        return ESP_ERR_NOT_SUPPORTED;
    }
#endif

    wifi_ps_type_t wifi_power_save = WIFI_PS_MIN_MODEM;
    if (mode == POWER_MANAGER_ACTIVE) {
        wifi_power_save = WIFI_PS_NONE;
    } else if (mode == POWER_MANAGER_LIGHT_SLEEP) {
        wifi_power_save = WIFI_PS_MAX_MODEM;
    }
    esp_err_t wifi_result = esp_wifi_set_ps(wifi_power_save);
    if (wifi_result != ESP_OK && wifi_result != ESP_ERR_WIFI_NOT_INIT) {
        return wifi_result;
    }

    const bool was_initialized = manager->initialized;
    const power_manager_mode_t previous = manager->mode;
    manager->mode = mode;
    manager->initialized = true;
    if (was_initialized) {
        ESP_LOGI(TAG, "Power mode changed: %s -> %s",
                 power_manager_mode_name(previous),
                 power_manager_mode_name(mode));
    } else {
        ESP_LOGI(TAG, "Power profile initialized: %s",
             power_manager_mode_name(mode));
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

    if (manager->mode == POWER_MANAGER_LIGHT_SLEEP) {
        ESP_LOGI(TAG,
                 "Idle %u seconds; automatic light sleep enabled direct=%d pending=%d",
                 (unsigned)interval_seconds,
                 direct_connection_active,
                 sync_pending);
        vTaskDelay(pdMS_TO_TICKS(interval_seconds * 1000U));
        return ESP_OK;
    }

    if (manager->mode == POWER_MANAGER_DEEP_SLEEP) {
        if (direct_connection_active) {
            ESP_LOGW(TAG, "Deep sleep postponed by a direct connection");
            vTaskDelay(pdMS_TO_TICKS(interval_seconds * 1000U));
            return ESP_OK;
        }
        ESP_RETURN_ON_ERROR(
            esp_sleep_enable_timer_wakeup(
                (uint64_t)interval_seconds * 1000000ULL),
            TAG,
            "deep sleep timer");
        ESP_LOGW(TAG,
                 "Entering deep sleep for %u seconds; pending=%d",
                 (unsigned)interval_seconds,
                 sync_pending);
        esp_err_t stop_result = esp_wifi_stop();
        if (stop_result != ESP_OK) {
            ESP_LOGD(TAG, "Wi-Fi stop before deep sleep: %s",
                     esp_err_to_name(stop_result));
        }
        vTaskDelay(pdMS_TO_TICKS(100));
        esp_deep_sleep_start();
    }

    vTaskDelay(pdMS_TO_TICKS(interval_seconds * 1000U));
    return ESP_OK;
}
