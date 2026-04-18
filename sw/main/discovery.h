#pragma once

#include "esp_err.h"

/**
 * UDP broadcast discovery beacon.
 *
 * Periodically broadcasts a JSON line on UDP port 55502 containing the
 * board's hostname, IP, and the Modbus TCP port. Control software on the
 * same L2 subnet can listen on 55502 and pick up the board without having
 * to initiate mDNS.
 *
 * Wire format (one datagram per announcement):
 *   {"app":"traincontrol","name":"<host>","ip":"<a.b.c.d>","modbus":502}
 */
esp_err_t discovery_start(const char *hostname);
void      discovery_stop(void);
