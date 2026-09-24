#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "mqtt_client.h"
#include "nvs_flash.h"

#define WIFI_CONNECTED_BIT BIT0
#define MQTT_CONNECTED_BIT BIT1

typedef struct {
    float temperature_c;
    float humidity_pct;
    float soil_moisture_pct;
    uint32_t light_lux;
    uint8_t battery_pct;
    const char *pump_state;
} sensor_snapshot_t;

static const char *TAG = "SMARTFARM";
static EventGroupHandle_t connection_events;
static esp_mqtt_client_handle_t mqtt_client;
static char telemetry_topic[96];
static uint32_t boot_id;

/*
 * Demo data provider. Replace this function with physical sensor drivers while
 * keeping sensor_snapshot_t and the JSON contract unchanged.
 */
static sensor_snapshot_t read_sensor_snapshot(uint32_t sequence)
{
    sensor_snapshot_t sample = {
        .temperature_c = 28.0f + (sequence % 10) * 0.2f,
        .humidity_pct = 70.0f + (sequence % 8) * 0.5f,
        .soil_moisture_pct = 40.0f + (sequence % 6) * 0.7f,
        .light_lux = 12000U + (sequence % 5) * 300U,
        .battery_pct = (uint8_t)(85U - (sequence % 5)),
        .pump_state = "off",
    };

    return sample;
}

static void wifi_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_ERROR_CHECK(esp_wifi_connect());
    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(connection_events, WIFI_CONNECTED_BIT);
        ESP_LOGW(TAG, "Wi-Fi disconnected; reconnecting");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = event_data;
        ESP_LOGI(TAG, "Wi-Fi connected, IP: " IPSTR,
                 IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(connection_events, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init(void)
{
    connection_events = xEventGroupCreate();
    ESP_ERROR_CHECK(connection_events == NULL ? ESP_ERR_NO_MEM : ESP_OK);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_netif_create_default_wifi_sta() == NULL
                        ? ESP_FAIL
                        : ESP_OK);

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_config));

    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_SMARTFARM_WIFI_SSID,
            .password = CONFIG_SMARTFARM_WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void mqtt_event_handler(
    void *handler_args,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    const esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        mqtt_client = event->client;
        xEventGroupSetBits(connection_events, MQTT_CONNECTED_BIT);
        ESP_LOGI(TAG, "MQTT connected");
        break;
    case MQTT_EVENT_DISCONNECTED:
        xEventGroupClearBits(connection_events, MQTT_CONNECTED_BIT);
        ESP_LOGW(TAG, "MQTT disconnected");
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error");
        break;
    default:
        break;
    }
}

static void mqtt_init(void)
{
    const esp_mqtt_client_config_t mqtt_config = {
        .broker.address.uri = CONFIG_SMARTFARM_MQTT_BROKER_URI,
        .credentials.username = CONFIG_SMARTFARM_MQTT_USERNAME,
        .credentials.authentication.password =
            CONFIG_SMARTFARM_MQTT_PASSWORD,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_config);
    ESP_ERROR_CHECK(mqtt_client == NULL ? ESP_FAIL : ESP_OK);
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(
        mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt_client));
}

static void telemetry_task(void *arg)
{
    uint32_t sequence = 1;

    while (true) {
        EventBits_t bits = xEventGroupGetBits(connection_events);

        if ((bits & MQTT_CONNECTED_BIT) != 0) {
            const sensor_snapshot_t sample =
                read_sensor_snapshot(sequence);
            char payload[320];

            int length = snprintf(
                payload,
                sizeof(payload),
                "{\"device_id\":\"%s\","
                "\"boot_id\":\"%08" PRIx32 "\","
                "\"seq\":%" PRIu32 ","
                "\"temperature_c\":%.1f,"
                "\"humidity_pct\":%.1f,"
                "\"soil_moisture_pct\":%.1f,"
                "\"light_lux\":%" PRIu32 ","
                "\"battery_pct\":%u,"
                "\"pump_state\":\"%s\"}",
                CONFIG_SMARTFARM_DEVICE_ID,
                boot_id,
                sequence,
                sample.temperature_c,
                sample.humidity_pct,
                sample.soil_moisture_pct,
                sample.light_lux,
                sample.battery_pct,
                sample.pump_state);

            if (length < 0 || length >= (int)sizeof(payload)) {
                ESP_LOGE(TAG, "Telemetry payload is too large");
            } else {
                int message_id = esp_mqtt_client_publish(
                    mqtt_client, telemetry_topic, payload, 0, 1, 0);

                if (message_id >= 0) {
                    ESP_LOGI(TAG,
                             "Published boot=%08" PRIx32
                             " seq=%" PRIu32 " message_id=%d",
                             boot_id,
                             sequence,
                             message_id);
                    sequence++;
                } else {
                    ESP_LOGE(TAG, "MQTT publish failed: %d", message_id);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(
            CONFIG_SMARTFARM_TELEMETRY_INTERVAL_SECONDS * 1000));
    }
}

void app_main(void)
{
    esp_err_t nvs_result = nvs_flash_init();

    if (nvs_result == ESP_ERR_NVS_NO_FREE_PAGES ||
        nvs_result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_result);

    if (CONFIG_SMARTFARM_WIFI_SSID[0] == '\0' ||
        CONFIG_SMARTFARM_WIFI_PASSWORD[0] == '\0' ||
        CONFIG_SMARTFARM_MQTT_BROKER_URI[0] == '\0' ||
        CONFIG_SMARTFARM_MQTT_USERNAME[0] == '\0' ||
        CONFIG_SMARTFARM_MQTT_PASSWORD[0] == '\0' ||
        CONFIG_SMARTFARM_DEVICE_ID[0] == '\0') {
        ESP_LOGE(TAG, "Missing configuration; run idf.py menuconfig");
        return;
    }

    int topic_length = snprintf(
        telemetry_topic,
        sizeof(telemetry_topic),
        "smartfarm/%s/telemetry",
        CONFIG_SMARTFARM_DEVICE_ID);
    ESP_ERROR_CHECK(
        topic_length < 0 || topic_length >= (int)sizeof(telemetry_topic)
            ? ESP_ERR_INVALID_SIZE
            : ESP_OK);

    boot_id = esp_random();
    wifi_init();

    xEventGroupWaitBits(connection_events,
                        WIFI_CONNECTED_BIT,
                        pdFALSE,
                        pdTRUE,
                        portMAX_DELAY);

    mqtt_init();
    BaseType_t task_result = xTaskCreate(
        telemetry_task, "telemetry_task", 4096, NULL, 5, NULL);
    ESP_ERROR_CHECK(task_result == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
