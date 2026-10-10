#include "data_manager.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#include "esp_timer.h"

#define SAEMS_VALID_TIME_EPOCH 1704067200

static void format_json_float(
    char *output, size_t output_size, bool valid, float value)
{
    if (valid) {
        (void)snprintf(output, output_size, "%.1f", value);
    } else {
        (void)snprintf(output, output_size, "null");
    }
}

static bool format_timestamp(
    char *output, size_t output_size, const char **timestamp_source)
{
    time_t now = time(NULL);
    if (now < SAEMS_VALID_TIME_EPOCH) {
        (void)snprintf(output, output_size, "null");
        *timestamp_source = "unsynced";
        return false;
    }

    struct tm utc_time;
    if (gmtime_r(&now, &utc_time) == NULL ||
        strftime(output, output_size, "\"%Y-%m-%dT%H:%M:%SZ\"", &utc_time) ==
            0) {
        (void)snprintf(output, output_size, "null");
        *timestamp_source = "unsynced";
        return false;
    }

    *timestamp_source = "system";
    return true;
}

static const char *aggregate_sensor_health(const sensor_snapshot_t *snapshot)
{
    const sensor_health_t health[] = {
        snapshot->dht20_health,
        snapshot->bh1750_health,
        snapshot->soil_health,
    };
    size_t ok_count = 0;
    size_t unavailable_count = 0;

    for (size_t index = 0; index < sizeof(health) / sizeof(health[0]); index++) {
        if (health[index] == SENSOR_HEALTH_OK) {
            ok_count++;
        } else if (health[index] == SENSOR_HEALTH_UNAVAILABLE) {
            unavailable_count++;
        }
    }

    if (ok_count == sizeof(health) / sizeof(health[0])) {
        return "ok";
    }
    if (unavailable_count == sizeof(health) / sizeof(health[0])) {
        return "unavailable";
    }
    return "degraded";
}

esp_err_t data_manager_build_telemetry(
    const sensor_snapshot_t *snapshot,
    const telemetry_metadata_t *metadata,
    char *payload,
    size_t payload_size)
{
    if (snapshot == NULL || metadata == NULL || payload == NULL ||
        payload_size == 0 || metadata->device_id == NULL ||
        metadata->pump_state == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    char temperature_json[24];
    char humidity_json[24];
    char soil_json[24];
    char light_json[24];
    char soil_raw_json[16];
    char timestamp_json[32];
    const char *timestamp_source = NULL;

    format_json_float(temperature_json,
                      sizeof(temperature_json),
                      snapshot->temperature_valid,
                      snapshot->temperature_c);
    format_json_float(humidity_json,
                      sizeof(humidity_json),
                      snapshot->humidity_valid,
                      snapshot->humidity_pct);
    format_json_float(soil_json,
                      sizeof(soil_json),
                      snapshot->soil_moisture_valid,
                      snapshot->soil_moisture_pct);
    format_json_float(light_json,
                      sizeof(light_json),
                      snapshot->light_valid,
                      snapshot->light_lux);
    (void)format_timestamp(
        timestamp_json, sizeof(timestamp_json), &timestamp_source);
    if (snapshot->soil_raw_valid) {
        (void)snprintf(soil_raw_json, sizeof(soil_raw_json), "%d", snapshot->soil_raw);
    } else {
        (void)snprintf(soil_raw_json, sizeof(soil_raw_json), "null");
    }

    const uint64_t uptime_ms = (uint64_t)esp_timer_get_time() / 1000ULL;
    int length = snprintf(
        payload,
        payload_size,
        "{\"schema_version\":%d,"
        "\"device_id\":\"%s\","
        "\"boot_id\":\"%08" PRIx32 "\","
        "\"seq\":%" PRIu32 ","
        "\"timestamp\":%s,"
        "\"timestamp_source\":\"%s\","
        "\"uptime_ms\":%" PRIu64 ","
        "\"data_mode\":\"real\","
        "\"sensor_health\":\"%s\","
        "\"sensor_status\":{"
        "\"dht20\":\"%s\","
        "\"bh1750\":\"%s\","
        "\"soil\":\"%s\"},"
        "\"temperature_c\":%s,"
        "\"humidity_pct\":%s,"
        "\"soil_moisture_pct\":%s,"
        "\"soil_raw\":%s,"
        "\"soil_digital\":%s,"
        "\"light_lux\":%s,"
        "\"battery_pct\":null,"
        "\"pump_state\":\"%s\"}",
        SAEMS_TELEMETRY_SCHEMA_VERSION,
        metadata->device_id,
        metadata->boot_id,
        metadata->sequence,
        timestamp_json,
        timestamp_source,
        uptime_ms,
        aggregate_sensor_health(snapshot),
        sensor_health_to_string(snapshot->dht20_health),
        sensor_health_to_string(snapshot->bh1750_health),
        sensor_health_to_string(snapshot->soil_health),
        temperature_json,
        humidity_json,
        soil_json,
        soil_raw_json,
        snapshot->soil_raw_valid ? (snapshot->soil_digital ? "true" : "false") : "null",
        light_json,
        metadata->pump_state);

    if (length < 0) {
        return ESP_FAIL;
    }
    if ((size_t)length >= payload_size) {
        payload[0] = '\0';
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}
