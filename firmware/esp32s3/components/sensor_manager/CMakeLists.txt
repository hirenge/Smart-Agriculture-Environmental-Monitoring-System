idf_component_register(
    SRCS "sensor_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES
        bh1750
        dht20
        esp_driver_i2c
        soil_moisture
)
