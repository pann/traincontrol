#include "fakes_state.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "event_log.h"
#include <string.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>

fake_state_t g_fake;

/* Storage for fake event log (declared extern in fakes/event_log.h) */
fake_event_entry_t g_fake_events[FAKE_EVENT_LOG_MAX];
int g_fake_event_count;

void fake_state_reset(void)
{
    memset(&g_fake, 0, sizeof(g_fake));
    fake_event_log_reset();
}

/* --- Event log fakes --- */

esp_err_t event_log_init(void)
{
    g_fake_event_count = 0;
    return ESP_OK;
}

void fake_event_log_reset(void)
{
    g_fake_event_count = 0;
}

void event_log(event_category_t category, const char *fmt, ...)
{
    if (g_fake_event_count < FAKE_EVENT_LOG_MAX) {
        g_fake_events[g_fake_event_count].category = category;
        va_list args;
        va_start(args, fmt);
        vsnprintf(g_fake_events[g_fake_event_count].message,
                  FAKE_EVENT_MSG_LEN, fmt, args);
        va_end(args);
        g_fake_event_count++;
    }
}

void fake_nvs_preset(const char *key, const char *value)
{
    if (g_fake.nvs_store_count >= FAKE_NVS_MAX_ENTRIES) return;
    fake_nvs_entry_t *e = &g_fake.nvs_store[g_fake.nvs_store_count++];
    strncpy(e->key, key, FAKE_NVS_MAX_KEY_LEN - 1);
    strncpy(e->value, value, FAKE_NVS_MAX_VAL_LEN - 1);
}

/* --- GPIO fakes --- */

esp_err_t gpio_config(const gpio_config_t *cfg)
{
    (void)cfg;
    return ESP_OK;
}

esp_err_t gpio_set_level(gpio_num_t pin, uint32_t level)
{
    if (g_fake.gpio_history_count < FAKE_GPIO_HISTORY_SIZE) {
        g_fake.gpio_history[g_fake.gpio_history_count].pin = pin;
        g_fake.gpio_history[g_fake.gpio_history_count].level = level;
        g_fake.gpio_history_count++;
    }
    return ESP_OK;
}

esp_err_t gpio_install_isr_service(int flags)
{
    (void)flags;
    return ESP_OK;
}

esp_err_t gpio_isr_handler_add(gpio_num_t pin, gpio_isr_t handler, void *arg)
{
    (void)pin;
    (void)handler;
    (void)arg;
    return ESP_OK;
}

/* --- esp_timer fakes --- */

/* Dummy timer struct so that esp_timer_handle_t (pointer) is dereferenceable */
struct esp_timer {
    int id;
};

static struct esp_timer s_fake_timers[4];

esp_err_t esp_timer_create(const esp_timer_create_args_t *args, esp_timer_handle_t *out)
{
    int idx = g_fake.timer_create_calls;
    g_fake.timer_create_calls++;
    if (idx < 4) {
        s_fake_timers[idx].id = idx;
        *out = &s_fake_timers[idx];
        if (args->callback && g_fake.timer_callback_count < 4) {
            g_fake.timer_callbacks[g_fake.timer_callback_count++] = args->callback;
        }
    }
    return ESP_OK;
}

esp_err_t esp_timer_start_once(esp_timer_handle_t timer, uint64_t timeout_us)
{
    (void)timer;
    g_fake.timer_start_once_calls++;
    g_fake.last_timer_timeout_us = timeout_us;
    return ESP_OK;
}

/* --- NVS fakes --- */

esp_err_t nvs_flash_init(void)
{
    return ESP_OK;
}

esp_err_t nvs_flash_erase(void)
{
    return ESP_OK;
}

esp_err_t nvs_open(const char *namespace_name, nvs_open_mode_t open_mode, nvs_handle_t *out_handle)
{
    (void)namespace_name;
    (void)open_mode;
    *out_handle = 1;
    return ESP_OK;
}

void nvs_close(nvs_handle_t handle)
{
    (void)handle;
}

esp_err_t nvs_get_str(nvs_handle_t handle, const char *key, char *out_value, size_t *length)
{
    (void)handle;
    for (int i = 0; i < g_fake.nvs_store_count; i++) {
        if (strcmp(g_fake.nvs_store[i].key, key) == 0) {
            size_t vlen = strlen(g_fake.nvs_store[i].value) + 1;
            if (*length >= vlen) {
                memcpy(out_value, g_fake.nvs_store[i].value, vlen);
            }
            *length = vlen;
            return ESP_OK;
        }
    }
    return ESP_ERR_NVS_NOT_FOUND;
}

esp_err_t nvs_set_str(nvs_handle_t handle, const char *key, const char *value)
{
    (void)handle;
    /* Update existing or add new */
    for (int i = 0; i < g_fake.nvs_store_count; i++) {
        if (strcmp(g_fake.nvs_store[i].key, key) == 0) {
            strncpy(g_fake.nvs_store[i].value, value, FAKE_NVS_MAX_VAL_LEN - 1);
            g_fake.nvs_store[i].value[FAKE_NVS_MAX_VAL_LEN - 1] = '\0';
            return ESP_OK;
        }
    }
    fake_nvs_preset(key, value);
    return ESP_OK;
}

esp_err_t nvs_commit(nvs_handle_t handle)
{
    (void)handle;
    return ESP_OK;
}

esp_err_t nvs_erase_all(nvs_handle_t handle)
{
    (void)handle;
    g_fake.nvs_store_count = 0;
    return ESP_OK;
}
