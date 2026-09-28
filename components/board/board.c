#include "board.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "BOARD";
static SemaphoreHandle_t s_i2c_mutex = NULL;
static i2c_bus_handle_t s_i2c_bus = NULL;

void board_i2c_lock(void)
{
    if (s_i2c_mutex != NULL) {
        xSemaphoreTake(s_i2c_mutex, portMAX_DELAY);
    }
}

void board_i2c_unlock(void)
{
    if (s_i2c_mutex != NULL) {
        xSemaphoreGive(s_i2c_mutex);
    }
}

i2c_bus_handle_t board_get_i2c_bus(void)
{
    return s_i2c_bus;
}

esp_err_t board_init(void)
{
    esp_err_t err = gpio_reset_pin(BOARD_RELAY_GPIO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset relay GPIO: %s", esp_err_to_name(err));
        return err;
    }

    gpio_set_direction(BOARD_RELAY_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(BOARD_RELAY_GPIO, 0);

    if (s_i2c_mutex == NULL) {
        s_i2c_mutex = xSemaphoreCreateMutex();
        if (s_i2c_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create I2C mutex");
            return ESP_FAIL;
        }
    }

    if (s_i2c_bus == NULL) {
        i2c_config_t i2c_conf = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = BOARD_I2C_SDA_GPIO,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_io_num = BOARD_I2C_SCL_GPIO,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master.clk_speed = 100000,
        };

        s_i2c_bus = i2c_bus_create(I2C_NUM_0, &i2c_conf);
        if (s_i2c_bus == NULL) {
            ESP_LOGE(TAG, "Failed to create I2C bus");
            return ESP_FAIL;
        }
    }

    ESP_LOGI(TAG, "Board initialized: relay on GPIO %d, I2C bus ready", BOARD_RELAY_GPIO);
    return ESP_OK;
}
