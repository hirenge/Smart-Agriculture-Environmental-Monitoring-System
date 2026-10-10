#include "soil_moisture.h"
#include "soil_calibration.h"

#include "driver/gpio.h"

#define SOIL_ANALOG_CHANNEL ADC_CHANNEL_3
#define SOIL_DIGITAL_GPIO GPIO_NUM_5
#define SOIL_ADC_SAMPLES 16

esp_err_t soil_moisture_init(
    soil_moisture_t *sensor, int dry_raw, int wet_raw)
{
    if (sensor == NULL || dry_raw == wet_raw) {
        return ESP_ERR_INVALID_ARG;
    }

    sensor->dry_raw = dry_raw;
    sensor->wet_raw = wet_raw;

    const adc_oneshot_unit_init_cfg_t adc_config = {
        .unit_id = ADC_UNIT_1,
        .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    esp_err_t result = adc_oneshot_new_unit(&adc_config, &sensor->adc);
    if (result != ESP_OK) {
        return result;
    }

    const adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    result = adc_oneshot_config_channel(
        sensor->adc, SOIL_ANALOG_CHANNEL, &channel_config);
    if (result != ESP_OK) {
        return result;
    }

    const gpio_config_t digital_config = {
        .pin_bit_mask = 1ULL << SOIL_DIGITAL_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    return gpio_config(&digital_config);
}

esp_err_t soil_moisture_read(
    soil_moisture_t *sensor,
    float *moisture_pct,
    int *raw_value,
    bool *digital_level)
{
    if (sensor == NULL || sensor->adc == NULL || moisture_pct == NULL ||
        raw_value == NULL || digital_level == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    int sum = 0;
    for (int sample = 0; sample < SOIL_ADC_SAMPLES; sample++) {
        int raw = 0;
        esp_err_t result = adc_oneshot_read(
            sensor->adc, SOIL_ANALOG_CHANNEL, &raw);
        if (result != ESP_OK) {
            return result;
        }
        sum += raw;
    }

    *raw_value = sum / SOIL_ADC_SAMPLES;
    *digital_level = gpio_get_level(SOIL_DIGITAL_GPIO) != 0;

    *moisture_pct = soil_calibration_percent(
        *raw_value, sensor->dry_raw, sensor->wet_raw);
    return ESP_OK;
}
