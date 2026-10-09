idf_component_register(
    SRCS "ap_server.c" "ble_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES bt esp_http_server
)
