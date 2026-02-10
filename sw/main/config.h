#pragma once

#include "esp_err.h"
#include <stdbool.h>

/**
 * Initialise the config module (reads NVS).
 */
esp_err_t config_init(void);

/**
 * Check if WiFi credentials are stored.
 */
bool config_has_wifi_credentials(void);

/**
 * Get stored WiFi SSID. Returns empty string if not set.
 */
const char *config_get_wifi_ssid(void);

/**
 * Get stored WiFi password. Returns empty string if not set.
 */
const char *config_get_wifi_password(void);

/**
 * Store WiFi credentials in NVS.
 */
esp_err_t config_set_wifi_credentials(const char *ssid, const char *password);

/**
 * Erase all stored config (factory reset).
 */
esp_err_t config_factory_reset(void);
