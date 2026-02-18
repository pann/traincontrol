#pragma once

#include "esp_err.h"

/**
 * Initialise provisioning subsystem.
 * - Always starts USB serial CLI (wifi_set, wifi_status, factory_reset).
 * - If no WiFi credentials stored: also starts SoftAP captive portal.
 */
esp_err_t provisioning_init(void);

/**
 * Start SoftAP captive portal for WiFi credential entry.
 * AP SSID: TrainCtrl-XXXX (XXXX = last 4 hex of MAC).
 */
esp_err_t provisioning_start_softap(void);

/**
 * Stop SoftAP captive portal and HTTP server.
 */
esp_err_t provisioning_stop_softap(void);
