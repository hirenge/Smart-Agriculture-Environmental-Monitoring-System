idf_component_register(
    SRCS "firmware_tasks.c" "sensor_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES dht20 soil_moisture light_sensor relay
)
