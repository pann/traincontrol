#pragma once

#include "esp_err.h"
#include <stdbool.h>

/**
 * Initialise WiFi subsystem (netif, event loop, event handlers).
 * If stored credentials exist, automatically begins STA connection.
 * If no credentials, WiFi stays idle until provisioning provides them.
 */
esp_err_t wifi_manager_init(void);

/**
 * Start STA mode connection using stored credentials.
 * Call after credentials have been stored via config_set_wifi_credentials().
 */
esp_err_t wifi_manager_start_sta(void);

/**
 * Stop WiFi (disconnect STA, unregister mDNS).
 */
esp_err_t wifi_manager_stop(void);

/**
 * Returns true if STA is connected and has an IP address.
 */
bool wifi_manager_is_connected(void);

/**
 * Get current IP address as a string (e.g. "192.168.1.42").
 * Returns empty string if not connected.
 */
const char *wifi_manager_get_ip_str(void);
