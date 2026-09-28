idf_component_register(
    SRCS "light_sensor.c"
    INCLUDE_DIRS "include"
    REQUIRES board driver espressif__i2c_bus
)
