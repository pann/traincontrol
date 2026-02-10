#pragma once

#include "esp_err.h"

/**
 * Initialise provisioning subsystem.
 * - If no WiFi credentials stored: start SoftAP captive portal.
 * - Always: start USB CDC/ACM CLI for diagnostics.
 */
esp_err_t provisioning_init(void);
