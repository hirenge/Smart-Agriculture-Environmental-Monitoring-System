idf_component_register(
    SRCS "network_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES config_manager esp_event esp_netif esp_wifi
)
