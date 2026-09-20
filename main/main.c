#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "i2c_bus.h"
#include "aht20.h"

#define AHT20_SCL_IO        18
#define AHT20_SDA_IO        17
#define I2C_MASTER_NUM      I2C_NUM_0
#define I2C_MASTER_FREQ_HZ  100000

#define SOIL_MOISTURE_ADC_GPIO  4
#define SOIL_MOISTURE_DO_GPIO   5

static const char *TAG = "AGRI";

void app_main(void)
{
    // 1) Khởi tạo bus I2C cho AHT20
    i2c_config_t i2c_bus_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = AHT20_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = AHT20_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    i2c_bus_handle_t i2c_bus = i2c_bus_create(I2C_MASTER_NUM, &i2c_bus_conf);
    if (i2c_bus == NULL) {
        ESP_LOGE(TAG, "Failed to create I2C bus");
        return;
    }

    // 2) Khởi tạo cảm biến AHT20
    aht20_i2c_config_t i2c_conf = {
        .bus_inst = i2c_bus,
        .i2c_addr = AHT20_ADDRRES_0,
    };

    aht20_dev_handle_t handle = NULL;
    esp_err_t ret = aht20_new_sensor(&i2c_conf, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init AHT20: %s", esp_err_to_name(ret));
        return;
    }

    // 3) Khởi tạo ADC oneshot cho soil moisture
    adc_oneshot_unit_handle_t adc_handle = NULL;
    adc_oneshot_unit_init_cfg_t adc_init_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    ret = adc_oneshot_new_unit(&adc_init_cfg, &adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init ADC unit: %s", esp_err_to_name(ret));
        return;
    }

    adc_oneshot_chan_cfg_t adc_chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ret = adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_3, &adc_chan_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to config ADC channel: %s", esp_err_to_name(ret));
        return;
    }

    // 4) Cài đặt chân DO của module LM393
    gpio_set_direction(SOIL_MOISTURE_DO_GPIO, GPIO_MODE_INPUT);

    while (1) {
        uint32_t temperature_raw = 0;
        uint32_t humidity_raw = 0;
        float temperature = 0.0f;
        float humidity = 0.0f;

        ret = aht20_read_temperature_humidity(handle,
                                              &temperature_raw,
                                              &temperature,
                                              &humidity_raw,
                                              &humidity);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Temp: %.2f C | Humidity: %.2f %%", temperature, humidity);
        } else {
            ESP_LOGE(TAG, "AHT20 read failed: %s", esp_err_to_name(ret));
        }

        int adc_raw = 0;
        ret = adc_oneshot_read(adc_handle, ADC_CHANNEL_3, &adc_raw);
        if (ret == ESP_OK) {
            float moisture_percent = ((float)adc_raw / 4095.0f) * 100.0f;
            int do_state = gpio_get_level(SOIL_MOISTURE_DO_GPIO);
            ESP_LOGI(TAG, "Soil ADC raw=%d, moisture=%.1f%%, D0=%d",
                     adc_raw, moisture_percent, do_state);
        } else {
            ESP_LOGE(TAG, "Soil ADC read failed: %s", esp_err_to_name(ret));
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}