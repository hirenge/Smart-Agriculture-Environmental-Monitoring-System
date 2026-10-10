idf_component_register(
    SRCS "soil_moisture.c"
    REQUIRES esp_adc esp_driver_gpio
    INCLUDE_DIRS "include"
)
