idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES board dht20 soil_moisture light_sensor relay app
)
