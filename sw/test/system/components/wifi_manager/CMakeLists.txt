# Wrapper component that compiles wifi_manager.c from the production source.
set(PROD_MAIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../main")

idf_component_register(
    SRCS "${PROD_MAIN_DIR}/wifi_manager.c"
    INCLUDE_DIRS "${PROD_MAIN_DIR}"
    PRIV_REQUIRES esp_wifi esp_event esp_netif esp_hw_support mdns config event_log
)
