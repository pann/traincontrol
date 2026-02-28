# Wrapper component that compiles config.c from the production source.
set(PROD_MAIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../main")

idf_component_register(
    SRCS "${PROD_MAIN_DIR}/config.c"
    INCLUDE_DIRS "${PROD_MAIN_DIR}"
    PRIV_REQUIRES nvs_flash event_log
)
