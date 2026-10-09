idf_component_register(
    SRCS "dht20.c"
    REQUIRES esp_driver_i2c
    INCLUDE_DIRS "include"
)
