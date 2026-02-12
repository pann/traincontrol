# Wrapper component that compiles event_log.c from the production source.
set(PROD_MAIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../main")

idf_component_register(
    SRCS "${PROD_MAIN_DIR}/event_log.c"
    INCLUDE_DIRS "${PROD_MAIN_DIR}"
)
