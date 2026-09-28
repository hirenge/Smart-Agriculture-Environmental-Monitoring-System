#include "relay.h"

#include "board.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "RELAY";
static bool s_initialized = false;
static bool s_state = false;

esp_err_t relay_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    gpio_reset_pin(BOARD_RELAY_GPIO);
    gpio_set_direction(BOARD_RELAY_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(BOARD_RELAY_GPIO, 0);

    s_state = false;
    s_initialized = true;
    ESP_LOGI(TAG, "Relay initialized");
    return ESP_OK;
}

esp_err_t relay_set(bool enabled)
{
    if (!s_initialized) {
        esp_err_t ret = relay_init();
        if (ret != ESP_OK) {
            return ret;
        }
    }

    gpio_set_level(BOARD_RELAY_GPIO, enabled ? 1 : 0);
    s_state = enabled;
    ESP_LOGI(TAG, "Relay set to %s", enabled ? "ON" : "OFF");
    return ESP_OK;
}

esp_err_t relay_get_state(bool *enabled)
{
    if (enabled == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *enabled = s_state;
    return ESP_OK;
}
