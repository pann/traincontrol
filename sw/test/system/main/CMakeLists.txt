idf_component_register(
    SRCS "test_main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES motor_control event_log console led_strip
)
