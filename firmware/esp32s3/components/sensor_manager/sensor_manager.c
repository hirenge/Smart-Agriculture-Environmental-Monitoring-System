#include "sensor_manager.h"

#include <string.h>
#include <math.h>

#include "esp_log.h"

#define SENSOR_MANAGER_I2C_FREQUENCY_HZ 100000

static const char *TAG = "SENSOR_MANAGER";

static void log_probe_result(
    i2c_master_bus_handle_t bus, uint8_t address, const char *name)
{
    esp_err_t result = i2c_master_probe(bus, address, 100);
    if (result == ESP_OK) {
        ESP_LOGI(TAG, "%s detected at I2C address 0x%02X", name, address);
    } else {
        ESP_LOGW(TAG,
                 "%s did not acknowledge I2C address 0x%02X: %s",
                 name,
                 address,
                 esp_err_to_name(result));
    }
}

const char *sensor_health_to_string(sensor_health_t health)
{
    switch (health) {
    case SENSOR_HEALTH_OK:
        return "ok";
    case SENSOR_HEALTH_ERROR:
        return "error";
    case SENSOR_HEALTH_UNCALIBRATED:
        return "uncalibrated";
    case SENSOR_HEALTH_UNAVAILABLE:
    default:
        return "unavailable";
    }
}

esp_err_t sensor_manager_init(
    sensor_manager_t *manager, const sensor_manager_config_t *config)
{
    if (manager == NULL || config == NULL ||
        !GPIO_IS_VALID_GPIO(config->i2c_sda) ||
        !GPIO_IS_VALID_GPIO(config->i2c_scl)) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(manager, 0, sizeof(*manager));
    manager->soil_enabled = config->soil_enabled;
    manager->soil_calibrated = config->soil_calibrated;

    const i2c_master_bus_config_t bus_config = {
        .i2c_port = config->i2c_port,
        .sda_io_num = config->i2c_sda,
        .scl_io_num = config->i2c_scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t bus_result = i2c_new_master_bus(
        &bus_config, &manager->i2c_bus);
    if (bus_result != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed: %s", esp_err_to_name(bus_result));
    } else {
        ESP_LOGI(TAG,
                 "I2C bus ready: SDA=GPIO%d SCL=GPIO%d frequency=%d Hz",
                 config->i2c_sda,
                 config->i2c_scl,
                 SENSOR_MANAGER_I2C_FREQUENCY_HZ);

        log_probe_result(manager->i2c_bus, 0x38, "DHT20");
        log_probe_result(
            manager->i2c_bus, config->bh1750_address, "BH1750");

        esp_err_t result = dht20_init(manager->i2c_bus, &manager->dht20);
        manager->dht20_ready = result == ESP_OK;
        if (manager->dht20_ready) {
            ESP_LOGI(TAG, "DHT20 ready");
        } else {
            ESP_LOGW(TAG, "DHT20 unavailable: %s", esp_err_to_name(result));
        }

        result = bh1750_init(
            manager->i2c_bus, config->bh1750_address, &manager->bh1750);
        manager->bh1750_ready = result == ESP_OK;
        if (manager->bh1750_ready) {
            ESP_LOGI(TAG, "BH1750 ready");
        } else {
            ESP_LOGW(TAG, "BH1750 unavailable: %s", esp_err_to_name(result));
        }
    }

    if (manager->soil_enabled) {
        esp_err_t result = soil_moisture_init(
            &manager->soil, config->soil_dry_raw, config->soil_wet_raw);
        manager->soil_ready = result == ESP_OK;
        if (!manager->soil_ready) {
            ESP_LOGW(TAG, "LM393 unavailable: %s", esp_err_to_name(result));
        } else {
            ESP_LOGI(TAG, "LM393 ready: AO=GPIO4 DO=GPIO5");
        }
    } else {
        ESP_LOGW(TAG,
                 "LM393 disabled until probe installation and dry/wet "
                 "calibration; soil values will be null");
    }

    manager->initialized = true;
    return ESP_OK;
}

esp_err_t sensor_manager_read_all(
    sensor_manager_t *manager, sensor_snapshot_t *snapshot)
{
    if (manager == NULL || snapshot == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!manager->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->dht20_health = manager->dht20_ready
                                 ? SENSOR_HEALTH_ERROR
                                 : SENSOR_HEALTH_UNAVAILABLE;
    snapshot->bh1750_health = manager->bh1750_ready
                                  ? SENSOR_HEALTH_ERROR
                                  : SENSOR_HEALTH_UNAVAILABLE;
    snapshot->soil_health = manager->soil_ready
                                ? SENSOR_HEALTH_ERROR
                                : SENSOR_HEALTH_UNAVAILABLE;

    if (manager->dht20_ready) {
        esp_err_t result = dht20_read(
            &manager->dht20,
            &snapshot->temperature_c,
            &snapshot->humidity_pct);
        snapshot->temperature_valid = result == ESP_OK;
        snapshot->humidity_valid = result == ESP_OK;
        snapshot->dht20_health = result == ESP_OK
                                     ? SENSOR_HEALTH_OK
                                     : SENSOR_HEALTH_ERROR;
        if (result != ESP_OK) {
            ESP_LOGW(TAG, "DHT20 read failed: %s", esp_err_to_name(result));
        }
    }

    if (manager->bh1750_ready) {
        esp_err_t result = bh1750_read_lux(
            &manager->bh1750, &snapshot->light_lux);
        snapshot->light_valid = result == ESP_OK;
        snapshot->bh1750_health = result == ESP_OK
                                      ? SENSOR_HEALTH_OK
                                      : SENSOR_HEALTH_ERROR;
        if (result != ESP_OK) {
            ESP_LOGW(TAG, "BH1750 read failed: %s", esp_err_to_name(result));
        }
    }

    if (manager->soil_ready) {
        esp_err_t result = soil_moisture_read(
            &manager->soil,
            &snapshot->soil_moisture_pct,
            &snapshot->soil_raw,
            &snapshot->soil_digital);
        snapshot->soil_raw_valid = result == ESP_OK;
        snapshot->soil_moisture_valid = result == ESP_OK && manager->soil_calibrated &&
                                        isfinite(snapshot->soil_moisture_pct);
        snapshot->soil_health = result == ESP_OK
                                    ? (manager->soil_calibrated
                                           ? (snapshot->soil_moisture_valid
                                                  ? SENSOR_HEALTH_OK
                                                  : SENSOR_HEALTH_ERROR)
                                           : SENSOR_HEALTH_UNCALIBRATED)
                                    : SENSOR_HEALTH_ERROR;
        if (result == ESP_OK) {
            if (snapshot->soil_moisture_valid) {
                ESP_LOGI(TAG, "LM393 raw=%d pct=%.1f DO=%d",
                         snapshot->soil_raw, snapshot->soil_moisture_pct,
                         snapshot->soil_digital);
            } else {
                ESP_LOGI(TAG, "LM393 raw=%d DO=%d calibration=pending; pct=null",
                         snapshot->soil_raw, snapshot->soil_digital);
            }
        } else {
            ESP_LOGW(TAG, "LM393 read failed: %s", esp_err_to_name(result));
        }
    }

    return ESP_OK;
}
