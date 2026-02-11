#pragma once

#include <stdint.h>

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
} fake_state_t;

extern fake_state_t g_fake;

void fake_state_reset(void);

/* NVS helpers for test setup */
void fake_nvs_preset(const char *key, const char *value);
