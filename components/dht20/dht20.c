#include "dht20.h"

#include "board.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "i2c_bus.h"
#include "aht20.h"

static const char *TAG = "DHT20";
static aht20_dev_handle_t s_handle = NULL;

esp_err_t dht20_init(void)
{
    if (s_handle != NULL) {
        return ESP_OK;
    }

    i2c_bus_handle_t bus = board_get_i2c_bus();
    if (bus == NULL) {
        ESP_LOGE(TAG, "I2C bus not initialized yet");
        return ESP_FAIL;
    }

    aht20_i2c_config_t i2c_conf = {
        .bus_inst = bus,
        .i2c_addr = AHT20_ADDRRES_0,
    };

    esp_err_t ret = aht20_new_sensor(&i2c_conf, &s_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init AHT20 sensor: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "DHT20 initialized successfully");
    return ESP_OK;
}

esp_err_t dht20_read(dht20_data_t *data)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_handle == NULL) {
        esp_err_t ret = dht20_init();
        if (ret != ESP_OK) {
            return ret;
        }
    }

    uint32_t temperature_raw = 0;
    uint32_t humidity_raw = 0;
    float temperature = 0.0f;
    float humidity = 0.0f;

    esp_err_t ret = aht20_read_temperature_humidity(s_handle,
                                                   &temperature_raw,
                                                   &temperature,
                                                   &humidity_raw,
                                                   &humidity);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "DHT20 read failed: %s", esp_err_to_name(ret));
        return ret;
    }

    data->temperature_c = temperature;
    data->humidity_percent = humidity;

    return ESP_OK;
}
