#include "bh1750.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define BH1750_ADDRESS_LOW 0x23
#define BH1750_ADDRESS_HIGH 0x5C
#define BH1750_POWER_ON 0x01
#define BH1750_ONE_TIME_HIGH_RESOLUTION 0x20
#define BH1750_I2C_TIMEOUT_MS 100
#define BH1750_CONVERSION_TIME_MS 180

esp_err_t bh1750_init(
    i2c_master_bus_handle_t bus, uint8_t address, bh1750_t *sensor)
{
    if (bus == NULL || sensor == NULL ||
        (address != BH1750_ADDRESS_LOW && address != BH1750_ADDRESS_HIGH)) {
        return ESP_ERR_INVALID_ARG;
    }

    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = 100000,
    };

    esp_err_t result = i2c_master_bus_add_device(
        bus, &device_config, &sensor->device);
    if (result != ESP_OK) {
        return result;
    }

    const uint8_t command = BH1750_POWER_ON;
    return i2c_master_transmit(
        sensor->device, &command, sizeof(command), BH1750_I2C_TIMEOUT_MS);
}

esp_err_t bh1750_read_lux(bh1750_t *sensor, float *light_lux)
{
    if (sensor == NULL || sensor->device == NULL || light_lux == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t command = BH1750_ONE_TIME_HIGH_RESOLUTION;
    esp_err_t result = i2c_master_transmit(
        sensor->device, &command, sizeof(command), BH1750_I2C_TIMEOUT_MS);
    if (result != ESP_OK) {
        return result;
    }

    vTaskDelay(pdMS_TO_TICKS(BH1750_CONVERSION_TIME_MS));

    uint8_t data[2] = {0};
    result = i2c_master_receive(
        sensor->device, data, sizeof(data), BH1750_I2C_TIMEOUT_MS);
    if (result != ESP_OK) {
        return result;
    }

    const uint16_t raw = ((uint16_t)data[0] << 8) | data[1];
    *light_lux = (float)raw / 1.2f;
    return ESP_OK;
}
