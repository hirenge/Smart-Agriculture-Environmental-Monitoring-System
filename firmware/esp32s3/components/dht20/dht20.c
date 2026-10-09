#include "dht20.h"

#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define DHT20_I2C_ADDRESS 0x38
#define DHT20_I2C_TIMEOUT_MS 100
#define DHT20_STATUS_BUSY (1U << 7)
#define DHT20_STATUS_CALIBRATED (1U << 3)

static uint8_t dht20_crc8(const uint8_t *data, size_t length)
{
    uint8_t crc = 0xFF;

    for (size_t byte = 0; byte < length; byte++) {
        crc ^= data[byte];
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 0x80U) != 0U
                      ? (uint8_t)((crc << 1) ^ 0x31U)
                      : (uint8_t)(crc << 1);
        }
    }

    return crc;
}

esp_err_t dht20_init(i2c_master_bus_handle_t bus, dht20_t *sensor)
{
    if (bus == NULL || sensor == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = DHT20_I2C_ADDRESS,
        .scl_speed_hz = 100000,
    };

    esp_err_t result = i2c_master_bus_add_device(
        bus, &device_config, &sensor->device);
    if (result != ESP_OK) {
        return result;
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    uint8_t status = 0;
    result = i2c_master_receive(
        sensor->device, &status, sizeof(status), DHT20_I2C_TIMEOUT_MS);
    if (result != ESP_OK) {
        return result;
    }

    return (status & DHT20_STATUS_CALIBRATED) != 0U
               ? ESP_OK
               : ESP_ERR_INVALID_STATE;
}

esp_err_t dht20_read(dht20_t *sensor, float *temperature_c, float *humidity_pct)
{
    if (sensor == NULL || sensor->device == NULL ||
        temperature_c == NULL || humidity_pct == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const uint8_t trigger[] = {0xAC, 0x33, 0x00};
    esp_err_t result = i2c_master_transmit(
        sensor->device, trigger, sizeof(trigger), DHT20_I2C_TIMEOUT_MS);
    if (result != ESP_OK) {
        return result;
    }

    uint8_t data[7] = {0};
    for (int attempt = 0; attempt < 3; attempt++) {
        vTaskDelay(pdMS_TO_TICKS(attempt == 0 ? 80 : 20));
        result = i2c_master_receive(
            sensor->device, data, sizeof(data), DHT20_I2C_TIMEOUT_MS);
        if (result != ESP_OK) {
            return result;
        }
        if ((data[0] & DHT20_STATUS_BUSY) == 0U) {
            break;
        }
    }

    if ((data[0] & DHT20_STATUS_BUSY) != 0U) {
        return ESP_ERR_TIMEOUT;
    }
    if ((data[0] & DHT20_STATUS_CALIBRATED) == 0U) {
        return ESP_ERR_INVALID_STATE;
    }
    if (dht20_crc8(data, 6) != data[6]) {
        return ESP_ERR_INVALID_CRC;
    }

    const uint32_t humidity_raw =
        ((uint32_t)data[1] << 12) |
        ((uint32_t)data[2] << 4) |
        ((uint32_t)data[3] >> 4);
    const uint32_t temperature_raw =
        (((uint32_t)data[3] & 0x0FU) << 16) |
        ((uint32_t)data[4] << 8) |
        (uint32_t)data[5];

    *humidity_pct = (float)humidity_raw * 100.0f / 1048576.0f;
    *temperature_c = (float)temperature_raw * 200.0f / 1048576.0f - 50.0f;
    return ESP_OK;
}
