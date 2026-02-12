#pragma once

#include "esp_err.h"

/**
 * Event categories for serial monitoring.
 *
 * All modules log significant events through event_log() so that
 * the serial console shows a unified stream of system activity.
 */
typedef enum {
    EVT_SYSTEM,
    EVT_MOTOR,
    EVT_WIFI,
    EVT_MODBUS,
    EVT_CONFIG,
    EVT_CATEGORY_COUNT
} event_category_t;

/**
 * Initialise the event logging subsystem.
 */
esp_err_t event_log_init(void);

/**
 * Log a system event.
 *
 * Output format on serial:
 *   I (timestamp) event: [MOTOR] speed_set speed=500
 *   I (timestamp) event: [WIFI]  connected ssid=MyNetwork
 *
 * @param category  Event category (determines the tag prefix)
 * @param fmt       printf-style format string
 * @param ...       Format arguments
 */
void event_log(event_category_t category, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));
