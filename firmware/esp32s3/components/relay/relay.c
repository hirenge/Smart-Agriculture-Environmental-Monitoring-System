#include "relay.h"

#include <stddef.h>

#include "driver/gpio.h"

static const gpio_num_t relay_gpios[] = {GPIO_NUM_10, GPIO_NUM_11};

static int relay_output_level(bool active_low, bool enabled)
{
    return active_low ? !enabled : enabled;
}

esp_err_t relay_init(relay_controller_t *controller, bool active_low)
{
    if (controller == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    controller->active_low = active_low;
    controller->enabled[0] = false;
    controller->enabled[1] = false;

    for (size_t index = 0; index < 2; index++) {
        esp_err_t result = gpio_set_level(
            relay_gpios[index], relay_output_level(active_low, false));
        if (result != ESP_OK) {
            return result;
        }
    }

    const gpio_config_t config = {
        .pin_bit_mask = (1ULL << GPIO_NUM_10) | (1ULL << GPIO_NUM_11),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    esp_err_t result = gpio_config(&config);
    if (result != ESP_OK) {
        return result;
    }

    esp_err_t channel_1_result =
        relay_set(controller, RELAY_CHANNEL_1, false);
    esp_err_t channel_2_result =
        relay_set(controller, RELAY_CHANNEL_2, false);
    return channel_1_result != ESP_OK ? channel_1_result : channel_2_result;
}

esp_err_t relay_set(
    relay_controller_t *controller, relay_channel_t channel, bool enabled)
{
    if (controller == NULL || channel < RELAY_CHANNEL_1 ||
        channel > RELAY_CHANNEL_2) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t result = gpio_set_level(
        relay_gpios[channel],
        relay_output_level(controller->active_low, enabled));
    if (result == ESP_OK) {
        controller->enabled[channel] = enabled;
    }
    return result;
}

bool relay_is_enabled(
    const relay_controller_t *controller, relay_channel_t channel)
{
    return controller != NULL && channel >= RELAY_CHANNEL_1 &&
           channel <= RELAY_CHANNEL_2 &&
           controller->enabled[channel];
}
