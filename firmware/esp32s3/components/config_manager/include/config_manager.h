#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

#define SAEMS_DEVICE_ID_MAX_LEN 32
#define SAEMS_WIFI_SSID_MAX_LEN 32
#define SAEMS_WIFI_PASSWORD_MAX_LEN 64
#define SAEMS_URI_MAX_LEN 128
#define SAEMS_MQTT_USERNAME_MAX_LEN 64
#define SAEMS_MQTT_PASSWORD_MAX_LEN 64

typedef enum {
    SAEMS_POWER_MODE_DIRECT = 0,
    SAEMS_POWER_MODE_LIGHT,
    SAEMS_POWER_MODE_BATTERY,
} saems_power_mode_t;

typedef struct {
    char device_id[SAEMS_DEVICE_ID_MAX_LEN + 1];
    char wifi_ssid[SAEMS_WIFI_SSID_MAX_LEN + 1];
    char wifi_password[SAEMS_WIFI_PASSWORD_MAX_LEN + 1];
    char mqtt_uri[SAEMS_URI_MAX_LEN + 1];
    char mqtt_username[SAEMS_MQTT_USERNAME_MAX_LEN + 1];
    char mqtt_password[SAEMS_MQTT_PASSWORD_MAX_LEN + 1];
    char ap_ssid[SAEMS_WIFI_SSID_MAX_LEN + 1];
    char ap_password[SAEMS_WIFI_PASSWORD_MAX_LEN + 1];
    uint32_t telemetry_interval_s;
    int soil_dry_raw;
    int soil_wet_raw;
    bool soil_enabled;
    bool soil_calibrated;
    bool ap_enabled;
    bool ble_enabled;
    bool power_save_enabled;
    saems_power_mode_t power_mode;
} saems_config_t;

typedef struct {
    saems_config_t current;
    void *mutex;
    bool initialized;
} config_manager_t;

esp_err_t config_manager_init(
    config_manager_t *manager, const saems_config_t *defaults);
esp_err_t config_manager_get(
    config_manager_t *manager, saems_config_t *config);
esp_err_t config_manager_update_json(
    config_manager_t *manager,
    const char *json,
    size_t json_length);
esp_err_t config_manager_to_json(
    config_manager_t *manager,
    char *output,
    size_t output_size);
const char *saems_power_mode_name(saems_power_mode_t mode);
esp_err_t saems_power_mode_from_string(
    const char *value, saems_power_mode_t *mode);
