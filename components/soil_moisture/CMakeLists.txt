idf_component_register(
    SRCS "soil_moisture.c"
    INCLUDE_DIRS "include"
    REQUIRES board driver esp_adc
)
