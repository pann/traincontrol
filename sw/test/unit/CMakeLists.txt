cmake_minimum_required(VERSION 3.16)
project(traincontrol_unit_tests C)

# Unity test framework
include(FetchContent)
FetchContent_Declare(
    unity
    GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
    GIT_TAG        v2.6.0
)
FetchContent_MakeAvailable(unity)

set(FAKES_DIR ${CMAKE_CURRENT_SOURCE_DIR}/fakes)
set(PROD_DIR  ${CMAKE_CURRENT_SOURCE_DIR}/../../main)

# Common compile options
add_compile_options(-Wall -Wextra -Werror -g)

# --- test_motor_control ---
add_executable(test_motor_control
    test_motor_control.c
    fakes/fakes_state.c
)
target_include_directories(test_motor_control PRIVATE
    ${FAKES_DIR}    # Fake ESP-IDF headers (must come first)
    ${PROD_DIR}     # Production headers (motor_control.h)
)
target_link_libraries(test_motor_control PRIVATE unity)

# --- test_config ---
add_executable(test_config
    test_config.c
    fakes/fakes_state.c
)
target_include_directories(test_config PRIVATE
    ${FAKES_DIR}
    ${PROD_DIR}
)
target_link_libraries(test_config PRIVATE unity)

# CTest registration
enable_testing()
add_test(NAME motor_control COMMAND test_motor_control)
add_test(NAME config        COMMAND test_config)
