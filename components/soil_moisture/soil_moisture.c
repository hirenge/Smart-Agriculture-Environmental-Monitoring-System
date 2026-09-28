#include "soil_moisture.h"

#include "board.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "SOIL";
static adc_oneshot_unit_handle_t s_adc_handle = NULL;
static bool s_initialized = false;

esp_err_t soil_moisture_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
    };

    esp_err_t ret = adc_oneshot_new_unit(&init_cfg, &s_adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init ADC unit: %s", esp_err_to_name(ret));
        return ret;
    }

    adc_oneshot_chan_cfg_t ch_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };

    ret = adc_oneshot_config_channel(s_adc_handle, BOARD_SOIL_ADC_CH, &ch_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to config soil ADC channel: %s", esp_err_to_name(ret));
        return ret;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "Soil moisture ADC initialized");
    return ESP_OK;
}

esp_err_t soil_moisture_read(soil_moisture_data_t *data)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized) {
        esp_err_t ret = soil_moisture_init();
        if (ret != ESP_OK) {
            return ret;
        }
    }

    int raw = 0;
    esp_err_t ret = adc_oneshot_read(s_adc_handle, BOARD_SOIL_ADC_CH, &raw);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read soil ADC: %s", esp_err_to_name(ret));
        return ret;
    }

    data->raw_adc = raw;
    data->voltage_mv = ((float)raw / 4095.0f) * 3300.0f;
    data->moisture_percent = ((float)raw / 4095.0f) * 100.0f;

    return ESP_OK;
}
