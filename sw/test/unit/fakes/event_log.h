#pragma once

#include "esp_err.h"

typedef enum {
    EVT_SYSTEM,
    EVT_MOTOR,
    EVT_WIFI,
    EVT_MODBUS,
    EVT_CONFIG,
    EVT_CATEGORY_COUNT
} event_category_t;

#define FAKE_EVENT_LOG_MAX 64
#define FAKE_EVENT_MSG_LEN 128

typedef struct {
    event_category_t category;
    char message[FAKE_EVENT_MSG_LEN];
} fake_event_entry_t;

extern fake_event_entry_t g_fake_events[];
extern int g_fake_event_count;

esp_err_t event_log_init(void);
void fake_event_log_reset(void);

void event_log(event_category_t category, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
