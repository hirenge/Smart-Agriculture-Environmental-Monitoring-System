idf_component_register(
    SRCS "data_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES sensor_manager
    PRIV_REQUIRES esp_timer
)
