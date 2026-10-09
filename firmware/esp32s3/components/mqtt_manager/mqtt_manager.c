#include "mqtt_manager.h"

#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"

static const char *TAG = "MQTT_MANAGER";

static void mqtt_event_handler(
    void *handler_args,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    (void)event_base;
    mqtt_manager_t *manager = handler_args;
    const esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED: {
        manager->connected = true;
        int message_id = esp_mqtt_client_subscribe(
            manager->client, manager->command_topic, 1);
        ESP_LOGI(TAG,
                 "Connected; command subscription=%s msg_id=%d",
                 manager->command_topic,
                 message_id);
        if (manager->connection_callback != NULL) {
            manager->connection_callback(true, manager->callback_context);
        }
        break;
    }
    case MQTT_EVENT_DISCONNECTED:
        manager->connected = false;
        ESP_LOGW(TAG, "Disconnected");
        if (manager->connection_callback != NULL) {
            manager->connection_callback(false, manager->callback_context);
        }
        break;
    case MQTT_EVENT_PUBLISHED:
        if (manager->published_callback != NULL) {
            manager->published_callback(
                event->msg_id, manager->callback_context);
        }
        break;
    case MQTT_EVENT_DATA:
        if (event->topic_len == (int)strlen(manager->command_topic) &&
            strncmp(event->topic,
                    manager->command_topic,
                    (size_t)event->topic_len) == 0 &&
            manager->command_callback != NULL) {
            manager->command_callback(event->data,
                                      (size_t)event->data_len,
                                      manager->callback_context);
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "Transport error");
        break;
    default:
        break;
    }
}

esp_err_t mqtt_manager_init(
    mqtt_manager_t *manager,
    const saems_config_t *config,
    mqtt_manager_command_callback_t command_callback,
    mqtt_manager_published_callback_t published_callback,
    mqtt_manager_connection_callback_t connection_callback,
    void *callback_context)
{
    if (manager == NULL || config == NULL || config->device_id[0] == '\0' ||
        config->mqtt_uri[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    memset(manager, 0, sizeof(*manager));
    manager->command_callback = command_callback;
    manager->published_callback = published_callback;
    manager->connection_callback = connection_callback;
    manager->callback_context = callback_context;

    int length = snprintf(manager->telemetry_topic,
                          sizeof(manager->telemetry_topic),
                          "saems/%s/telemetry",
                          config->device_id);
    if (length < 0 || length >= (int)sizeof(manager->telemetry_topic)) {
        return ESP_ERR_INVALID_SIZE;
    }
    length = snprintf(manager->command_topic,
                      sizeof(manager->command_topic),
                      "saems/%s/commands",
                      config->device_id);
    if (length < 0 || length >= (int)sizeof(manager->command_topic)) {
        return ESP_ERR_INVALID_SIZE;
    }

    const esp_mqtt_client_config_t mqtt_config = {
        .broker.address.uri = config->mqtt_uri,
        .broker.verification.crt_bundle_attach =
            strncmp(config->mqtt_uri, "mqtts://", 8) == 0
                ? esp_crt_bundle_attach : NULL,
        .credentials.username = config->mqtt_username,
        .credentials.authentication.password = config->mqtt_password,
        .session.disable_clean_session = false,
        .network.reconnect_timeout_ms = 5000,
    };
    manager->client = esp_mqtt_client_init(&mqtt_config);
    if (manager->client == NULL) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t result = esp_mqtt_client_register_event(manager->client,
                                                     ESP_EVENT_ANY_ID,
                                                     mqtt_event_handler,
                                                     manager);
    if (result == ESP_OK) {
        result = esp_mqtt_client_start(manager->client);
    }
    if (result != ESP_OK) {
        (void)esp_mqtt_client_destroy(manager->client);
        manager->client = NULL;
        return result;
    }
    manager->initialized = true;
    return ESP_OK;
}

bool mqtt_manager_is_connected(mqtt_manager_t *manager)
{
    return manager != NULL && manager->initialized && manager->connected;
}

int mqtt_manager_publish(mqtt_manager_t *manager, const char *payload)
{
    if (!mqtt_manager_is_connected(manager) || payload == NULL) {
        return -1;
    }
    return esp_mqtt_client_publish(manager->client,
                                   manager->telemetry_topic,
                                   payload,
                                   0,
                                   1,
                                   0);
}
