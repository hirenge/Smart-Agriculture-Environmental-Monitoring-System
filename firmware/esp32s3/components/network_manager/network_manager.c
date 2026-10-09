#include "network_manager.h"

#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_wifi.h"

static const char *TAG = "NETWORK_MANAGER";

static void time_synchronized(struct timeval *time)
{
    ESP_LOGI(TAG, "SNTP synchronized: epoch=%lld", (long long)time->tv_sec);
}

static void network_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    network_manager_t *manager = arg;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        (void)esp_wifi_connect();
    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(
            manager->events, NETWORK_MANAGER_STA_CONNECTED_BIT);
        ESP_LOGW(TAG, "Wi-Fi STA disconnected; reconnecting");
        (void)esp_wifi_connect();
    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_AP_START) {
        xEventGroupSetBits(manager->events, NETWORK_MANAGER_AP_STARTED_BIT);
        ESP_LOGI(TAG, "Wi-Fi AP started at 192.168.4.1");
    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_AP_STOP) {
        xEventGroupClearBits(manager->events, NETWORK_MANAGER_AP_STARTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = event_data;
        xEventGroupSetBits(
            manager->events, NETWORK_MANAGER_STA_CONNECTED_BIT);
        ESP_LOGI(TAG,
                 "Wi-Fi STA connected, IP: " IPSTR,
                 IP2STR(&event->ip_info.ip));
        if (manager->sntp_initialized) {
            esp_err_t result = esp_netif_sntp_start();
            if (result != ESP_OK) {
                ESP_LOGW(TAG, "Start SNTP after IP failed: %s", esp_err_to_name(result));
            }
        }
    }
}

esp_err_t network_manager_init(
    network_manager_t *manager, const saems_config_t *config)
{
    if (manager == NULL || config == NULL ||
        (config->wifi_ssid[0] == '\0' && !config->ap_enabled)) {
        return ESP_ERR_INVALID_ARG;
    }
    memset(manager, 0, sizeof(*manager));
    manager->events = xEventGroupCreate();
    if (manager->events == NULL) {
        return ESP_ERR_NO_MEM;
    }
    manager->sta_enabled = config->wifi_ssid[0] != '\0';
    manager->ap_enabled = config->ap_enabled;

    esp_err_t result = esp_netif_init();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        return result;
    }
    result = esp_event_loop_create_default();
    if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) {
        return result;
    }
    if (manager->sta_enabled && esp_netif_create_default_wifi_sta() == NULL) {
        return ESP_FAIL;
    }
    if (manager->ap_enabled && esp_netif_create_default_wifi_ap() == NULL) {
        return ESP_FAIL;
    }

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init_config), TAG, "wifi init");
    ESP_RETURN_ON_ERROR(
        esp_event_handler_register(
            WIFI_EVENT, ESP_EVENT_ANY_ID, network_event_handler, manager),
        TAG,
        "register Wi-Fi event");
    ESP_RETURN_ON_ERROR(
        esp_event_handler_register(
            IP_EVENT, IP_EVENT_STA_GOT_IP, network_event_handler, manager),
        TAG,
        "register IP event");

    wifi_mode_t mode = manager->sta_enabled
                           ? (manager->ap_enabled ? WIFI_MODE_APSTA
                                                  : WIFI_MODE_STA)
                           : WIFI_MODE_AP;
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(mode), TAG, "set Wi-Fi mode");
    if (manager->sta_enabled) {
        wifi_config_t station_config = {0};
        const size_t station_ssid_length =
            strnlen(config->wifi_ssid, sizeof(station_config.sta.ssid) - 1);
        const size_t station_password_length =
            strnlen(config->wifi_password,
                    sizeof(station_config.sta.password) - 1);
        memcpy(station_config.sta.ssid,
               config->wifi_ssid,
               station_ssid_length);
        memcpy(station_config.sta.password,
               config->wifi_password,
               station_password_length);
        station_config.sta.threshold.authmode =
            station_password_length == 0 ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
        ESP_RETURN_ON_ERROR(
            esp_wifi_set_config(WIFI_IF_STA, &station_config),
            TAG,
            "set STA config");
    }

    if (manager->ap_enabled) {
        if (strlen(config->ap_password) < 8) {
            ESP_LOGE(TAG, "AP password must contain at least 8 characters");
            return ESP_ERR_INVALID_ARG;
        }
        wifi_config_t ap_config = {0};
        const size_t ap_ssid_length =
            strnlen(config->ap_ssid, sizeof(ap_config.ap.ssid) - 1);
        const size_t ap_password_length =
            strnlen(config->ap_password,
                    sizeof(ap_config.ap.password) - 1);
        memcpy(ap_config.ap.ssid, config->ap_ssid, ap_ssid_length);
        memcpy(ap_config.ap.password,
               config->ap_password,
               ap_password_length);
        ap_config.ap.ssid_len = ap_ssid_length;
        ap_config.ap.channel = 1;
        ap_config.ap.max_connection = 4;
        ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
        ESP_RETURN_ON_ERROR(
            esp_wifi_set_config(WIFI_IF_AP, &ap_config),
            TAG,
            "set AP config");
        ESP_LOGI(TAG, "AP enabled: SSID=%s", config->ap_ssid);
    }

    if (manager->sta_enabled) {
        esp_sntp_config_t time_config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(
            3, ESP_SNTP_SERVER_LIST("time.cloudflare.com", "time.google.com", "pool.ntp.org"));
        time_config.start = false;
        time_config.server_from_dhcp = false;
        time_config.sync_cb = time_synchronized;
        ESP_RETURN_ON_ERROR(esp_netif_sntp_init(&time_config), TAG, "time sync init");
        manager->sntp_initialized = true;
    }
    ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "start Wi-Fi");
    if (manager->sta_enabled) {
        ESP_RETURN_ON_ERROR(
            esp_wifi_set_ps(config->power_save_enabled
                                ? WIFI_PS_MIN_MODEM
                                : WIFI_PS_NONE),
            TAG,
            "set Wi-Fi power save");
    }
    manager->initialized = true;
    return ESP_OK;
}

bool network_manager_is_connected(network_manager_t *manager)
{
    return manager != NULL && manager->initialized &&
           (xEventGroupGetBits(manager->events) &
            NETWORK_MANAGER_STA_CONNECTED_BIT) != 0;
}

bool network_manager_is_ap_active(network_manager_t *manager)
{
    return manager != NULL && manager->initialized &&
           (xEventGroupGetBits(manager->events) &
            NETWORK_MANAGER_AP_STARTED_BIT) != 0;
}

EventBits_t network_manager_wait_for_connection(
    network_manager_t *manager, TickType_t timeout)
{
    if (manager == NULL || !manager->initialized) {
        return 0;
    }
    return xEventGroupWaitBits(manager->events,
                               NETWORK_MANAGER_STA_CONNECTED_BIT,
                               pdFALSE,
                               pdTRUE,
                               timeout);
}
