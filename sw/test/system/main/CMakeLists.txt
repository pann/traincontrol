idf_component_register(
    SRCS "test_main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES motor_control modbus_server event_log console led_strip
                  wifi_manager provisioning config nvs_flash
)
