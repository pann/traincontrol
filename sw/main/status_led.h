#pragma once

#include "esp_err.h"

/**
 * Initialise the RGB status LED (WS2812 on GPIO 48).
 *
 * Starts a periodic task that updates the LED color based on motor state:
 *   - Green = forward, brightness proportional to speed
 *   - Red   = reverse, brightness proportional to speed
 *   - Off   = disabled or speed 0
 */
esp_err_t status_led_init(void);

/**
 * Force an immediate LED update (e.g. after a motor state change from CLI).
 */
void status_led_update(void);
