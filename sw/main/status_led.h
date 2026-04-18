#pragma once

#include "esp_err.h"
#include <stdint.h>

/**
 * Initialise the RGB status LED (WS2812 on GPIO 7).
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

/**
 * Override the LED with a fixed color. While an override is active the
 * motor-state task will NOT touch the LED. Used as a debug trace during
 * bring-up. Call status_led_clear_override() to restore motor-driven updates.
 */
void status_led_set_rgb(uint8_t r, uint8_t g, uint8_t b);

/**
 * Briefly show a color for `duration_ms`, then revert to whatever the
 * last status_led_set_rgb() call set. Useful for transient events
 * (e.g. incoming TCP connection).
 */
void status_led_flash_rgb(uint8_t r, uint8_t g, uint8_t b, uint32_t duration_ms);

/**
 * Release the override so the motor-state task resumes driving the LED.
 */
void status_led_clear_override(void);
