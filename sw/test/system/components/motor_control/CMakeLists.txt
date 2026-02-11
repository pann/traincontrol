# Wrapper component that compiles motor_control.c from the production source.
set(PROD_MAIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../main")

idf_component_register(
    SRCS "${PROD_MAIN_DIR}/motor_control.c"
    INCLUDE_DIRS "${PROD_MAIN_DIR}"
    PRIV_REQUIRES driver esp_timer
)
