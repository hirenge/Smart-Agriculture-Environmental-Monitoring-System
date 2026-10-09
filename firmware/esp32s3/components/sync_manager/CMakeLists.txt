idf_component_register(
    SRCS "sync_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES mqtt_manager storage_manager
)
