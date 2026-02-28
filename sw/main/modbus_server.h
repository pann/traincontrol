#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * Initialise the Modbus TCP server on port 502.
 */
esp_err_t modbus_server_init(void);

/**
 * Check if a Modbus TCP client is currently connected.
 */
bool modbus_server_client_connected(void);

/**
 * Get the total number of Modbus transactions processed.
 */
uint32_t modbus_server_get_transaction_count(void);

/**
 * Read/write registers locally (for CLI diagnostics).
 * These go through the same code path as the Modbus TCP protocol.
 */
uint16_t modbus_read_input_reg(uint16_t addr);
uint16_t modbus_read_holding_reg(uint16_t addr);
void modbus_write_holding_reg(uint16_t addr, uint16_t value);
bool modbus_read_coil(uint16_t addr);
void modbus_write_coil(uint16_t addr, bool value);
