# Wrapper component that compiles modbus_server.c from the production source.
set(PROD_MAIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../main")

idf_component_register(
    SRCS "${PROD_MAIN_DIR}/modbus_server.c"
    INCLUDE_DIRS "${PROD_MAIN_DIR}"
    PRIV_REQUIRES motor_control event_log wifi_manager lwip esp_timer
)
