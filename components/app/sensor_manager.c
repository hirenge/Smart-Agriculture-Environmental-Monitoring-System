#include "sensor_manager.h"

#include "esp_log.h"
#include "relay.h"

static const char *TAG = "SENSOR_MGR";

esp_err_t sensor_manager_init(void)
{
    esp_err_t ret = dht20_init();
    if (ret != ESP_OK) {
        return ret;
    }

    ret = soil_moisture_init();
    if (ret != ESP_OK) {
        return ret;
    }

    ret = light_sensor_init();
    if (ret != ESP_OK) {
        return ret;
    }

    ret = relay_init();
    if (ret != ESP_OK) {
        return ret;
    }

    ESP_LOGI(TAG, "Sensor manager initialized");
    return ESP_OK;
}

esp_err_t sensor_manager_sample(sensor_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = dht20_read(&snapshot->dht20);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = soil_moisture_read(&snapshot->soil);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = light_sensor_read(&snapshot->light);
    if (ret != ESP_OK) {
        return ret;
    }

    bool pump_state = false;
    relay_get_state(&pump_state);
    snapshot->pump_state = pump_state;

    return ESP_OK;
}
