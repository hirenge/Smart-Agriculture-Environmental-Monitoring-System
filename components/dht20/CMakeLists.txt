idf_component_register(
    SRCS "dht20.c"
    INCLUDE_DIRS "include"
    REQUIRES board driver espressif__aht20 espressif__i2c_bus
)
