#pragma once

#include "esp_err.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "i2c_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BOARD_I2C_SCL_GPIO      18
#define BOARD_I2C_SDA_GPIO      17
#define BOARD_RELAY_GPIO        12
#define BOARD_SOIL_ADC_CH       ADC_CHANNEL_3
#define BOARD_BH1750_I2C_ADDR   0x23

esp_err_t board_init(void);
void board_i2c_lock(void);
void board_i2c_unlock(void);
i2c_bus_handle_t board_get_i2c_bus(void);

#ifdef __cplusplus
}
#endif
