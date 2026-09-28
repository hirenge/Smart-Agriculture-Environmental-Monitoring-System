#include "light_sensor.h"

#include "board.h"
#include "bh1750.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "LIGHT";
static bool s_initialized = false;
static bh1750_dev_t s_sensor = {0};

esp_err_t light_sensor_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    board_i2c_lock();
    esp_err_t ret = bh1750_init_desc(&s_sensor, BOARD_BH1750_I2C_ADDR, I2C_NUM_0);
    if (ret == ESP_OK) {
        ret = bh1750_power_on(&s_sensor);
    }
    if (ret == ESP_OK) {
        ret = bh1750_setup(&s_sensor, BH1750_MODE_CONTINUOUS, BH1750_RES_HIGH);
    }
    board_i2c_unlock();

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize BH1750: %s", esp_err_to_name(ret));
        return ret;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "BH1750 light sensor initialized");
    return ESP_OK;
}

esp_err_t light_sensor_read(light_sensor_data_t *data)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized) {
        esp_err_t ret = light_sensor_init();
        if (ret != ESP_OK) {
            return ret;
        }
    }

    uint16_t lux = 0;
    board_i2c_lock();
    esp_err_t ret = bh1750_read(&s_sensor, &lux);
    board_i2c_unlock();

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read BH1750: %s", esp_err_to_name(ret));
        return ret;
    }

    data->raw = lux;
    data->lux = (float)lux;
    return ESP_OK;
}
