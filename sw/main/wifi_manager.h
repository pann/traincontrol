#pragma once

#include "esp_err.h"

/**
 * Initialise WiFi in station mode and connect using stored credentials.
 * If no credentials are stored, WiFi remains idle until provisioning
 * provides them.
 */
esp_err_t wifi_manager_init(void);
