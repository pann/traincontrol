#pragma once

#include <stdint.h>
#include <stdbool.h>

#define IRAM_ATTR  /* no-op on host */

#define FAKE_GPIO_HISTORY_SIZE 128
#define FAKE_NVS_MAX_ENTRIES   16
#define FAKE_NVS_MAX_KEY_LEN   16
#define FAKE_NVS_MAX_VAL_LEN   128

typedef struct {
    int pin;
    uint32_t level;
} fake_gpio_event_t;

typedef struct {
    char key[FAKE_NVS_MAX_KEY_LEN];
    char value[FAKE_NVS_MAX_VAL_LEN];
} fake_nvs_entry_t;

/* Forward-declare types used in tracking fields */
typedef void (*esp_event_handler_func_t)(void *event_handler_arg,
                                          const char *event_base,
                                          int32_t event_id,
                                          void *event_data);

typedef struct {
    /* GPIO tracking */
    fake_gpio_event_t gpio_history[FAKE_GPIO_HISTORY_SIZE];
    int gpio_history_count;

    /* esp_timer tracking */
    int timer_create_calls;
    int timer_start_once_calls;
    uint64_t last_timer_timeout_us;

    /* Stored timer callbacks for manual invocation in tests */
    void (*timer_callbacks[4])(void *arg);
    int timer_callback_count;

    /* NVS in-memory store */
    fake_nvs_entry_t nvs_store[FAKE_NVS_MAX_ENTRIES];
    int nvs_store_count;

    /* WiFi tracking */
    int wifi_mode;
    int wifi_init_calls;
    int wifi_start_calls;
    int wifi_stop_calls;
    int wifi_connect_calls;
    int wifi_set_config_calls;

    /* Event handler tracking */
    esp_event_handler_func_t event_handlers[8];
    int event_handler_count;

    /* MAC address (configurable for tests) */
    uint8_t fake_mac[6];

    /* mDNS tracking */
    int mdns_init_calls;
    char mdns_hostname[64];
    int mdns_service_add_calls;

    /* HTTP server tracking */
    int httpd_start_calls;
    int httpd_stop_calls;

    /* Console tracking */
    int console_cmd_register_calls;

    /* Config tracking (wifi credentials) */
    bool config_has_creds;
    char config_ssid[64];
    char config_pass[128];

    /* System tracking */
    int restart_calls;

    /* Motor control tracking (for modbus_server tests) */
    bool motor_enabled;
    bool motor_forward;
    uint16_t motor_speed;
    uint16_t motor_current_speed;
    int motor_estop_calls;

    /* Timer time (for esp_timer_get_time fake) */
    int64_t fake_timer_time_us;

    /* Task tracking */
    int task_create_calls;
} fake_state_t;

extern fake_state_t g_fake;

void fake_state_reset(void);

/* NVS helpers for test setup */
void fake_nvs_preset(const char *key, const char *value);

/* MAC address helper for tests */
void fake_set_mac(uint8_t b0, uint8_t b1, uint8_t b2,
                  uint8_t b3, uint8_t b4, uint8_t b5);
