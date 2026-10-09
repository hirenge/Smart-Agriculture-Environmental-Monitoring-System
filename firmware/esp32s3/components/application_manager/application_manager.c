#include "application_manager.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "sdkconfig.h"

#include "config_manager.h"
#include "data_manager.h"
#include "direct_connection.h"
#include "esp_app_desc.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mqtt_manager.h"
#include "network_manager.h"
#include "nvs.h"
#include "power_manager.h"
#include "relay.h"
#include "sensor_manager.h"
#include "storage_manager.h"
#include "sync_manager.h"

#define SAEMS_I2C_SDA GPIO_NUM_17
#define SAEMS_I2C_SCL GPIO_NUM_18
#define SAEMS_PAYLOAD_SIZE SAEMS_STORAGE_RECORD_SIZE
#define SAEMS_SEQUENCE_BLOCK_SIZE 1000000U
#define SAEMS_NVS_NAMESPACE "saems"
#define SAEMS_NVS_SEQUENCE_KEY "seq_next"

#ifdef CONFIG_SAEMS_BH1750_ADDRESS_HIGH
#define SAEMS_BH1750_ADDRESS 0x5C
#else
#define SAEMS_BH1750_ADDRESS 0x23
#endif

#ifdef CONFIG_SAEMS_RELAY_ACTIVE_LOW
#define SAEMS_RELAY_ACTIVE_LOW true
#else
#define SAEMS_RELAY_ACTIVE_LOW false
#endif

typedef enum {
    APP_STATE_BOOT = 0,
    APP_STATE_INIT,
    APP_STATE_READ_SENSOR,
    APP_STATE_PROCESS_DATA,
    APP_STATE_STORE_DATA,
    APP_STATE_SYNC_DATA,
    APP_STATE_IDLE,
    APP_STATE_ERROR,
} application_state_t;

typedef struct {
    application_state_t state;
    config_manager_t config_manager;
    sensor_manager_t sensor_manager;
    relay_controller_t relay;
    network_manager_t network_manager;
    mqtt_manager_t mqtt_manager;
    storage_manager_t storage_manager;
    sync_manager_t sync_manager;
    power_manager_t power_manager;
    ap_server_t ap_server;
    ble_manager_t ble_manager;
    esp_timer_handle_t pump_timer;
    SemaphoreHandle_t latest_mutex;
    saems_config_t config;
    char latest_payload[SAEMS_PAYLOAD_SIZE];
    uint32_t boot_id;
    uint32_t sequence;
    uint32_t sequence_limit;
    bool sensor_ready;
    bool relay_ready;
} application_context_t;

static const char *TAG = "APPLICATION_MANAGER";
static application_context_t application;

static const char *state_name(application_state_t state)
{
    switch (state) {
    case APP_STATE_BOOT:
        return "boot";
    case APP_STATE_INIT:
        return "init";
    case APP_STATE_READ_SENSOR:
        return "read_sensor";
    case APP_STATE_PROCESS_DATA:
        return "process_data";
    case APP_STATE_STORE_DATA:
        return "store_data";
    case APP_STATE_SYNC_DATA:
        return "sync_data";
    case APP_STATE_IDLE:
        return "idle";
    case APP_STATE_ERROR:
    default:
        return "error";
    }
}

static void transition(application_state_t state)
{
    application.state = state;
    ESP_LOGD(TAG, "State=%s", state_name(state));
}

static esp_err_t reserve_sequence_block(uint32_t *first_sequence)
{
    if (first_sequence == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    nvs_handle_t handle;
    esp_err_t result = nvs_open(
        SAEMS_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (result != ESP_OK) {
        return result;
    }
    uint32_t stored_next = 1;
    result = nvs_get_u32(handle, SAEMS_NVS_SEQUENCE_KEY, &stored_next);
    if (result == ESP_ERR_NVS_NOT_FOUND) {
        result = ESP_OK;
    }
    if (result == ESP_OK &&
        stored_next > UINT32_MAX - SAEMS_SEQUENCE_BLOCK_SIZE) {
        result = ESP_ERR_INVALID_STATE;
    }
    if (result == ESP_OK) {
        *first_sequence = stored_next;
        result = nvs_set_u32(handle,
                             SAEMS_NVS_SEQUENCE_KEY,
                             stored_next + SAEMS_SEQUENCE_BLOCK_SIZE);
    }
    if (result == ESP_OK) {
        result = nvs_commit(handle);
    }
    nvs_close(handle);
    return result;
}

static void pump_timeout(void *arg)
{
    (void)arg;
    if (application.relay_ready &&
        relay_is_enabled(&application.relay, RELAY_CHANNEL_1)) {
        (void)relay_set(&application.relay, RELAY_CHANNEL_1, false);
        ESP_LOGW(TAG, "Pump safety timeout; relay CH1 OFF");
    }
}

static esp_err_t set_pump(bool enabled, uint32_t requested_duration_s)
{
    if (!application.relay_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    (void)esp_timer_stop(application.pump_timer);
    ESP_RETURN_ON_ERROR(
        relay_set(&application.relay, RELAY_CHANNEL_1, enabled),
        TAG,
        "set pump relay");
    if (!enabled) {
        return ESP_OK;
    }
    uint32_t duration = requested_duration_s;
    if (duration == 0 || duration > CONFIG_SAEMS_RELAY_MAX_ON_SECONDS) {
        duration = CONFIG_SAEMS_RELAY_MAX_ON_SECONDS;
    }
    esp_err_t result = esp_timer_start_once(
        application.pump_timer, (uint64_t)duration * 1000000ULL);
    if (result != ESP_OK) {
        (void)relay_set(&application.relay, RELAY_CHANNEL_1, false);
    }
    return result;
}

static esp_err_t handle_command_json(
    void *context, const char *json, size_t json_length)
{
    (void)context;
    if (json == NULL || json_length == 0 || json_length >= 512) {
        return ESP_ERR_INVALID_ARG;
    }
    char document[512];
    memcpy(document, json, json_length);
    document[json_length] = '\0';
    const char *command = strstr(document, "\"command\"");
    bool valid_command = command == NULL || strstr(command, "\"pump\"") != NULL;
    const char *state = strstr(document, "\"state\"");
    bool enabled = false;
    bool valid_state = false;
    if (state != NULL) {
        state = strchr(state, ':');
        if (state != NULL) {
            if (strstr(state, "\"on\"") != NULL) {
                enabled = true;
                valid_state = true;
            } else if (strstr(state, "\"off\"") != NULL) {
                valid_state = true;
            }
        }
    }
    uint32_t duration_s = 0;
    const char *duration = strstr(document, "\"duration_s\"");
    if (duration != NULL && (duration = strchr(duration, ':')) != NULL) {
        char *end = NULL;
        long parsed = strtol(duration + 1, &end, 10);
        if (end != duration + 1 && parsed > 0 && parsed <= UINT32_MAX) {
            duration_s = (uint32_t)parsed;
        }
    }
    if (!valid_command || !valid_state) {
        return ESP_ERR_INVALID_ARG;
    }
    return set_pump(enabled, duration_s);
}

static esp_err_t provide_device_info(
    void *context, char *output, size_t output_size)
{
    (void)context;
    const esp_app_desc_t *description = esp_app_get_description();
    int length = snprintf(output,
                          output_size,
                          "{\"device_id\":\"%s\",\"project\":\"SAEMS\","
                          "\"firmware_version\":\"%s\","
                          "\"ble_protocol\":1,\"api_version\":1}",
                          application.config.device_id,
                          description->version);
    return length < 0 || (size_t)length >= output_size
               ? ESP_ERR_INVALID_SIZE
               : ESP_OK;
}

static esp_err_t provide_latest(
    void *context, char *output, size_t output_size)
{
    (void)context;
    xSemaphoreTake(application.latest_mutex, portMAX_DELAY);
    int length = snprintf(output,
                          output_size,
                          "%s",
                          application.latest_payload[0] == '\0'
                              ? "{}"
                              : application.latest_payload);
    xSemaphoreGive(application.latest_mutex);
    return length < 0 || (size_t)length >= output_size
               ? ESP_ERR_INVALID_SIZE
               : ESP_OK;
}

static esp_err_t provide_status(
    void *context, char *output, size_t output_size)
{
    (void)context;
    char latest[SAEMS_PAYLOAD_SIZE];
    ESP_RETURN_ON_ERROR(
        provide_latest(NULL, latest, sizeof(latest)), TAG, "latest telemetry");
    int length = snprintf(
        output,
        output_size,
        "{\"device_id\":\"%s\",\"online\":true,\"last_seen\":null,"
        "\"device_state\":\"%s\",\"server_connected\":%s,"
        "\"pending_records\":%u,\"telemetry\":%s}",
        application.config.device_id,
        state_name(application.state),
        mqtt_manager_is_connected(&application.mqtt_manager)
            ? "true"
            : "false",
        (unsigned)storage_manager_pending_count(
            &application.storage_manager),
        latest);
    return length < 0 || (size_t)length >= output_size
               ? ESP_ERR_INVALID_SIZE
               : ESP_OK;
}

static esp_err_t provide_history(
    void *context, size_t limit, char *output, size_t output_size)
{
    (void)context;
    return storage_manager_history_json(&application.storage_manager,
                                        limit,
                                        output,
                                        output_size);
}

static esp_err_t provide_config(
    void *context, char *output, size_t output_size)
{
    (void)context;
    return config_manager_to_json(
        &application.config_manager, output, output_size);
}

static esp_err_t update_config(
    void *context, const char *json, size_t json_length)
{
    (void)context;
    esp_err_t result = config_manager_update_json(
        &application.config_manager, json, json_length);
    if (result == ESP_OK) {
        (void)config_manager_get(
            &application.config_manager, &application.config);
        ESP_LOGI(TAG, "Runtime configuration updated and persisted");
    }
    return result;
}

static void mqtt_command(
    const char *payload, size_t payload_length, void *context)
{
    esp_err_t result = handle_command_json(context, payload, payload_length);
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "MQTT command rejected: %s", esp_err_to_name(result));
    }
}

static void mqtt_published(int message_id, void *context)
{
    (void)context;
    sync_manager_on_published(&application.sync_manager, message_id);
}

static void mqtt_connection(bool connected, void *context)
{
    (void)context;
    sync_manager_on_connection_changed(&application.sync_manager, connected);
}

static direct_connection_callbacks_t direct_callbacks(void)
{
    return (direct_connection_callbacks_t){
        .device_info = provide_device_info,
        .status = provide_status,
        .latest_telemetry = provide_latest,
        .telemetry_history = provide_history,
        .get_config = provide_config,
        .set_config = update_config,
        .command = handle_command_json,
        .context = &application,
    };
}

static void start_mqtt_when_ready(void)
{
    if (application.mqtt_manager.initialized ||
        application.config.wifi_ssid[0] == '\0' ||
        application.config.mqtt_uri[0] == '\0' ||
        !network_manager_is_connected(&application.network_manager)) {
        return;
    }
    if (strncmp(application.config.mqtt_uri, "mqtts://", 8) == 0 &&
        time(NULL) < 1704067200) {
        ESP_LOGW(TAG, "Waiting for SNTP before MQTT TLS; sampling/BLE/AP remain active");
        return;
    }
    esp_err_t result = mqtt_manager_init(&application.mqtt_manager,
                                        &application.config,
                                        mqtt_command,
                                        mqtt_published,
                                        mqtt_connection,
                                        &application);
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "MQTT initialization will retry: %s", esp_err_to_name(result));
    }
}

static void sampling_task(void *arg)
{
    (void)arg;
    while (true) {
        if (application.sequence >= application.sequence_limit) {
            esp_err_t result = reserve_sequence_block(&application.sequence);
            if (result != ESP_OK) {
                transition(APP_STATE_ERROR);
                ESP_LOGE(TAG, "Reserve sequence failed: %s", esp_err_to_name(result));
                vTaskDelete(NULL);
            }
            application.sequence_limit =
                application.sequence + SAEMS_SEQUENCE_BLOCK_SIZE;
        }

        transition(APP_STATE_READ_SENSOR);
        sensor_snapshot_t snapshot;
        esp_err_t result = application.sensor_ready
                               ? sensor_manager_read_all(
                                     &application.sensor_manager, &snapshot)
                               : ESP_ERR_INVALID_STATE;
        if (result != ESP_OK) {
            memset(&snapshot, 0, sizeof(snapshot));
            snapshot.dht20_health = SENSOR_HEALTH_UNAVAILABLE;
            snapshot.bh1750_health = SENSOR_HEALTH_UNAVAILABLE;
            snapshot.soil_health = SENSOR_HEALTH_UNAVAILABLE;
        }

        transition(APP_STATE_PROCESS_DATA);
        const char *pump_state = application.relay_ready
                                     ? (relay_is_enabled(&application.relay,
                                                         RELAY_CHANNEL_1)
                                            ? "on"
                                            : "off")
                                     : "unknown";
        const telemetry_metadata_t metadata = {
            .device_id = application.config.device_id,
            .boot_id = application.boot_id,
            .sequence = application.sequence,
            .pump_state = pump_state,
        };
        char payload[SAEMS_PAYLOAD_SIZE];
        result = data_manager_build_telemetry(
            &snapshot, &metadata, payload, sizeof(payload));
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "Build telemetry failed: %s", esp_err_to_name(result));
            transition(APP_STATE_ERROR);
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        xSemaphoreTake(application.latest_mutex, portMAX_DELAY);
        (void)snprintf(application.latest_payload,
                       sizeof(application.latest_payload),
                       "%s",
                       payload);
        xSemaphoreGive(application.latest_mutex);
        if (application.config.ble_enabled) {
            (void)ble_manager_notify_telemetry(
                &application.ble_manager, payload);
        }

        transition(APP_STATE_STORE_DATA);
        result = storage_manager_save(&application.storage_manager, payload);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "Persist telemetry failed: %s", esp_err_to_name(result));
        }
        /* A live BLE reading must not reuse a sequence if the offline queue
         * is full and that sample cannot be persisted. */
        application.sequence++;

        transition(APP_STATE_SYNC_DATA);
        start_mqtt_when_ready();
        sync_manager_kick(&application.sync_manager);
        ESP_LOGI(TAG,
                 "Sample captured: boot=%08" PRIx32 " seq=%" PRIu32
                 " pending=%u payload=%s",
                 application.boot_id,
                 metadata.sequence,
                 (unsigned)storage_manager_pending_count(
                     &application.storage_manager),
                 payload);

        transition(APP_STATE_IDLE);
        bool direct_active =
            ap_server_is_running(&application.ap_server) ||
            ble_manager_is_connected(&application.ble_manager);
        bool sync_pending = storage_manager_pending_count(
                                &application.storage_manager) > 0;
        (void)power_manager_idle(&application.power_manager,
                                 application.config.telemetry_interval_s,
                                 direct_active,
                                 sync_pending);
    }
}

esp_err_t application_manager_start(void)
{
    memset(&application, 0, sizeof(application));
    transition(APP_STATE_BOOT);
    application.latest_mutex = xSemaphoreCreateMutex();
    if (application.latest_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    saems_config_t defaults = {0};
    (void)snprintf(defaults.device_id,
                   sizeof(defaults.device_id),
                   "%s",
                   CONFIG_SAEMS_DEVICE_ID);
    (void)snprintf(defaults.wifi_ssid,
                   sizeof(defaults.wifi_ssid),
                   "%s",
                   CONFIG_SAEMS_WIFI_SSID);
    (void)snprintf(defaults.wifi_password,
                   sizeof(defaults.wifi_password),
                   "%s",
                   CONFIG_SAEMS_WIFI_PASSWORD);
    (void)snprintf(defaults.mqtt_uri,
                   sizeof(defaults.mqtt_uri),
                   "%s",
                   CONFIG_SAEMS_MQTT_BROKER_URI);
    (void)snprintf(defaults.mqtt_username,
                   sizeof(defaults.mqtt_username),
                   "%s",
                   CONFIG_SAEMS_MQTT_USERNAME);
    (void)snprintf(defaults.mqtt_password,
                   sizeof(defaults.mqtt_password),
                   "%s",
                   CONFIG_SAEMS_MQTT_PASSWORD);
    (void)snprintf(defaults.ap_ssid,
                   sizeof(defaults.ap_ssid),
                   "SAEMS-%s",
                   CONFIG_SAEMS_DEVICE_ID);
    (void)snprintf(defaults.ap_password,
                   sizeof(defaults.ap_password),
                   "%s",
                   CONFIG_SAEMS_AP_PASSWORD);
    defaults.telemetry_interval_s = CONFIG_SAEMS_TELEMETRY_INTERVAL_SECONDS;
#ifdef CONFIG_SAEMS_SOIL_SENSOR_ENABLED
    defaults.soil_enabled = true;
    defaults.soil_dry_raw = CONFIG_SAEMS_SOIL_DRY_RAW;
    defaults.soil_wet_raw = CONFIG_SAEMS_SOIL_WET_RAW;
#else
    defaults.soil_dry_raw = 3000;
    defaults.soil_wet_raw = 1200;
#endif
#ifdef CONFIG_SAEMS_AP_ENABLED
    defaults.ap_enabled = true;
#endif
#ifdef CONFIG_SAEMS_BLE_ENABLED
    defaults.ble_enabled = true;
#endif
#ifdef CONFIG_SAEMS_POWER_SAVE_ENABLED
    defaults.power_save_enabled = true;
#endif

    transition(APP_STATE_INIT);
    ESP_RETURN_ON_ERROR(
        config_manager_init(&application.config_manager, &defaults),
        TAG,
        "config manager");
    ESP_RETURN_ON_ERROR(config_manager_get(&application.config_manager,
                                           &application.config),
                        TAG,
                        "load config");
    if (application.config.device_id[0] == '\0') {
        ESP_LOGE(TAG, "Missing device configuration");
        return ESP_ERR_INVALID_STATE;
    }

    const sensor_manager_config_t sensor_config = {
        .i2c_port = I2C_NUM_0,
        .i2c_sda = SAEMS_I2C_SDA,
        .i2c_scl = SAEMS_I2C_SCL,
        .bh1750_address = SAEMS_BH1750_ADDRESS,
        .soil_enabled = application.config.soil_enabled,
        .soil_dry_raw = application.config.soil_dry_raw,
        .soil_wet_raw = application.config.soil_wet_raw,
    };
    esp_err_t result = sensor_manager_init(
        &application.sensor_manager, &sensor_config);
    application.sensor_ready = result == ESP_OK;
    if (result != ESP_OK) {
        ESP_LOGW(TAG, "Sensor Manager degraded: %s", esp_err_to_name(result));
    }
    result = relay_init(&application.relay, SAEMS_RELAY_ACTIVE_LOW);
    application.relay_ready = result == ESP_OK;
    ESP_RETURN_ON_ERROR(result, TAG, "relay");

    const esp_timer_create_args_t timer_args = {
        .callback = pump_timeout,
        .name = "pump_auto_off",
    };
    ESP_RETURN_ON_ERROR(
        esp_timer_create(&timer_args, &application.pump_timer),
        TAG,
        "pump timer");
    ESP_RETURN_ON_ERROR(
        storage_manager_init(&application.storage_manager),
        TAG,
        "storage manager");
    const bool station_configured = application.config.wifi_ssid[0] != '\0';
    const bool mqtt_configured = application.config.mqtt_uri[0] != '\0';
    if (station_configured || application.config.ap_enabled) {
        ESP_RETURN_ON_ERROR(
            network_manager_init(&application.network_manager,
                                 &application.config),
            TAG,
            "network manager");
    } else {
        ESP_LOGW(TAG,
                 "Wi-Fi STA is not configured; BLE/offline mode remains active");
    }
    ESP_RETURN_ON_ERROR(
        power_manager_init(
            &application.power_manager,
            station_configured && application.config.power_save_enabled
                ? POWER_MANAGER_MODEM_SAVE
                : POWER_MANAGER_ACTIVE),
        TAG,
        "power manager");

    ESP_RETURN_ON_ERROR(
        sync_manager_init(&application.sync_manager,
                          &application.storage_manager,
                          &application.mqtt_manager),
        TAG,
        "sync manager");
    if (!station_configured || !mqtt_configured) {
        ESP_LOGW(TAG,
                 "MQTT sync disabled until Wi-Fi and broker are configured");
    }

    const direct_connection_callbacks_t callbacks = direct_callbacks();
    if (application.config.ap_enabled) {
        ESP_RETURN_ON_ERROR(
            ap_server_start(&application.ap_server, &callbacks),
            TAG,
            "AP REST server");
    }
    if (application.config.ble_enabled) {
        char ble_name[32];
        (void)snprintf(ble_name,
                       sizeof(ble_name),
                       "SAEMS-%.25s",
                       application.config.device_id);
        ESP_RETURN_ON_ERROR(
            ble_manager_start(&application.ble_manager,
                              ble_name,
                              &callbacks),
            TAG,
            "BLE manager");
    }

    application.boot_id = esp_random();
    ESP_RETURN_ON_ERROR(
        reserve_sequence_block(&application.sequence),
        TAG,
        "reserve sequence");
    application.sequence_limit =
        application.sequence + SAEMS_SEQUENCE_BLOCK_SIZE;

    BaseType_t task_result = xTaskCreate(
        sampling_task, "saems_state_machine", 8192, NULL, 5, NULL);
    if (task_result != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG,
             "SAEMS application started: AP=%d BLE=%d pending=%u",
             application.config.ap_enabled,
             application.config.ble_enabled,
             (unsigned)storage_manager_pending_count(
                 &application.storage_manager));
    return ESP_OK;
}
