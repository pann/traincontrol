#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * Initialise motor control GPIOs, zero-crossing ISR, and phase-angle timer.
 */
esp_err_t motor_control_init(void);

/**
 * Set motor direction.
 * @param forward  true = forward (winding A), false = reverse (winding B)
 */
void motor_set_direction(bool forward);

/**
 * Set target speed (phase-angle).
 * @param speed  0-1000 (0 = stop, 1000 = full power)
 */
void motor_set_speed(uint16_t speed);

/**
 * Enable or disable motor output.
 */
void motor_set_enable(bool enable);

/**
 * Emergency stop — immediately disable motor output.
 */
void motor_emergency_stop(void);

/**
 * Get the current actual speed (accounts for ramp).
 */
uint16_t motor_get_current_speed(void);

/**
 * Get motor enabled state.
 */
bool motor_get_enable(void);

/**
 * Get motor direction.
 */
bool motor_get_direction_forward(void);
