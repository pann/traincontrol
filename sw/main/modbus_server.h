#pragma once

#include "esp_err.h"

/**
 * Initialise the Modbus TCP server on port 502.
 */
esp_err_t modbus_server_init(void);
