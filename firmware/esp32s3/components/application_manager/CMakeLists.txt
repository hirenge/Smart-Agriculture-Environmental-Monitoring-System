idf_component_register(
    SRCS "application_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES
        config_manager
        data_manager
        direct_connection
        esp_app_format
        esp_timer
        mqtt_manager
        network_manager
        nvs_flash
        power_manager
        relay
        sensor_manager
        storage_manager
        sync_manager
)
