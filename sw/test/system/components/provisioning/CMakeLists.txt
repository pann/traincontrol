# Wrapper component that compiles provisioning.c from the production source.
set(PROD_MAIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../main")

idf_component_register(
    SRCS "${PROD_MAIN_DIR}/provisioning.c"
    INCLUDE_DIRS "${PROD_MAIN_DIR}"
    PRIV_REQUIRES esp_wifi esp_event esp_netif esp_hw_support esp_http_server
                  esp_timer console config wifi_manager event_log
)
