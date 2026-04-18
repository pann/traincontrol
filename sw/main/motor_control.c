#include "motor_control.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "event_log.h"

static const char *TAG = "motor";

/* GPIO assignments — see high-level-design.md section 2.2 */
#define GPIO_MOTOR_FWD   GPIO_NUM_4
#define GPIO_MOTOR_REV   GPIO_NUM_5
#define GPIO_ZERO_CROSS  GPIO_NUM_6

/* Rail-voltage sense: +15V → R9 (100k) → VSENSE → R10 (27k) → GND,
 * VSENSE wired to ESP32-C3 IO3 = ADC1 channel 3. */
#define VSENSE_ADC_UNIT   ADC_UNIT_1
#define VSENSE_ADC_CHAN   ADC_CHANNEL_3
#define VSENSE_ADC_ATTEN  ADC_ATTEN_DB_12   /* full-scale ≈ 3.3 V */
#define VSENSE_DIVIDER_N  127               /* (R9 + R10) / R10 numerator   */
#define VSENSE_DIVIDER_D  27                /* (R9 + R10) / R10 denominator */

/* Phase-angle range in microseconds within a 10 ms half-cycle */
#define HALF_CYCLE_US    10000
#define FIRE_MIN_US       1000   /* earliest firing (max power) */
#define FIRE_MAX_US       9000   /* latest firing (min power)   */
#define GATE_PULSE_US       50   /* TRIAC gate pulse width      */

static volatile bool     s_enabled   = false;
static volatile bool     s_forward   = true;
static volatile uint16_t s_target    = 0;      /* 0-1000 */
static volatile uint16_t s_current   = 0;      /* 0-1000, after ramp */

static esp_timer_handle_t s_fire_timer;
static esp_timer_handle_t s_pulse_timer;

static adc_oneshot_unit_handle_t s_vsense_adc;
static adc_cali_handle_t         s_vsense_cali;

/* --- Forward declarations --- */
static void IRAM_ATTR zero_cross_isr(void *arg);
static void fire_timer_cb(void *arg);
static void pulse_end_cb(void *arg);

/* Convert speed (0-1000) to firing delay in microseconds.
 * speed 0    → no firing (motor off)
 * speed 1    → FIRE_MAX_US (late firing, minimum power)
 * speed 1000 → FIRE_MIN_US (early firing, maximum power)
 */
static uint32_t speed_to_delay_us(uint16_t speed)
{
    if (speed == 0) return 0;
    return FIRE_MAX_US - (uint32_t)(speed - 1) * (FIRE_MAX_US - FIRE_MIN_US) / 999;
}

/* Zero-crossing ISR — start the fire-delay timer */
static void IRAM_ATTR zero_cross_isr(void *arg)
{
    (void)arg;
    if (!s_enabled || s_current == 0) return;

    uint32_t delay = speed_to_delay_us(s_current);
    if (delay > 0) {
        esp_timer_start_once(s_fire_timer, delay);
    }
}

/* Fire timer callback — assert gate pulse on the active direction */
static void fire_timer_cb(void *arg)
{
    (void)arg;
    /* The XOR interlock guarantees HW safety, but we also only assert
     * one direction GPIO at a time in software. */
    if (s_forward) {
        gpio_set_level(GPIO_MOTOR_FWD, 1);
    } else {
        gpio_set_level(GPIO_MOTOR_REV, 1);
    }
    /* Start pulse-end timer to turn off the gate signal */
    esp_timer_start_once(s_pulse_timer, GATE_PULSE_US);
}

/* Pulse-end callback — de-assert both direction GPIOs */
static void pulse_end_cb(void *arg)
{
    (void)arg;
    gpio_set_level(GPIO_MOTOR_FWD, 0);
    gpio_set_level(GPIO_MOTOR_REV, 0);
}

esp_err_t motor_control_init(void)
{
    ESP_LOGI(TAG, "Initialising motor control");

    /* Configure direction GPIOs as outputs, default low */
    gpio_config_t out_conf = {
        .pin_bit_mask = (1ULL << GPIO_MOTOR_FWD) | (1ULL << GPIO_MOTOR_REV),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&out_conf);
    gpio_set_level(GPIO_MOTOR_FWD, 0);
    gpio_set_level(GPIO_MOTOR_REV, 0);

    /* Configure zero-crossing input with interrupt on falling edge */
    gpio_config_t zc_conf = {
        .pin_bit_mask = (1ULL << GPIO_ZERO_CROSS),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&zc_conf);

    /* Create one-shot timers for fire delay and gate pulse */
    const esp_timer_create_args_t fire_args = {
        .callback = fire_timer_cb,
        .name     = "triac_fire",
    };
    esp_timer_create(&fire_args, &s_fire_timer);

    const esp_timer_create_args_t pulse_args = {
        .callback = pulse_end_cb,
        .name     = "triac_pulse",
    };
    esp_timer_create(&pulse_args, &s_pulse_timer);

    /* Install GPIO ISR and attach zero-crossing handler */
    gpio_install_isr_service(0);
    gpio_isr_handler_add(GPIO_ZERO_CROSS, zero_cross_isr, NULL);

    /* Rail-voltage sense ADC */
    adc_oneshot_unit_init_cfg_t unit_cfg = { .unit_id = VSENSE_ADC_UNIT };
    adc_oneshot_new_unit(&unit_cfg, &s_vsense_adc);

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten    = VSENSE_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    adc_oneshot_config_channel(s_vsense_adc, VSENSE_ADC_CHAN, &chan_cfg);

    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id  = VSENSE_ADC_UNIT,
        .chan     = VSENSE_ADC_CHAN,
        .atten    = VSENSE_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    if (adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_vsense_cali) != ESP_OK) {
        ESP_LOGW(TAG, "ADC cali unavailable — rail voltage will be uncalibrated");
        s_vsense_cali = NULL;
    }

    event_log(EVT_MOTOR, "init complete");
    return ESP_OK;
}

void motor_set_direction(bool forward)
{
    /* De-assert both GPIOs before switching direction */
    gpio_set_level(GPIO_MOTOR_FWD, 0);
    gpio_set_level(GPIO_MOTOR_REV, 0);
    s_forward = forward;
    event_log(EVT_MOTOR, "dir=%s", forward ? "fwd" : "rev");
}

void motor_set_speed(uint16_t speed)
{
    if (speed > 1000) speed = 1000;
    s_target = speed;
    /* TODO: implement ramp — for now, set directly */
    s_current = speed;
    event_log(EVT_MOTOR, "speed=%u", speed);
}

void motor_set_enable(bool enable)
{
    s_enabled = enable;
    if (!enable) {
        gpio_set_level(GPIO_MOTOR_FWD, 0);
        gpio_set_level(GPIO_MOTOR_REV, 0);
        s_current = 0;
    }
    event_log(EVT_MOTOR, "enable=%d", enable);
}

void motor_emergency_stop(void)
{
    s_enabled = false;
    s_target  = 0;
    s_current = 0;
    gpio_set_level(GPIO_MOTOR_FWD, 0);
    gpio_set_level(GPIO_MOTOR_REV, 0);
    event_log(EVT_MOTOR, "emergency_stop");
}

uint16_t motor_get_current_speed(void) { return s_current; }
bool     motor_get_enable(void)        { return s_enabled; }
bool     motor_get_direction_forward(void) { return s_forward; }

uint16_t motor_get_rail_voltage_mv(void)
{
    if (!s_vsense_adc || !s_vsense_cali) return 0;

    int raw = 0;
    if (adc_oneshot_read(s_vsense_adc, VSENSE_ADC_CHAN, &raw) != ESP_OK) return 0;

    int adc_mv = 0;
    if (adc_cali_raw_to_voltage(s_vsense_cali, raw, &adc_mv) != ESP_OK) return 0;

    uint32_t rail_mv = (uint32_t)adc_mv * VSENSE_DIVIDER_N / VSENSE_DIVIDER_D;
    return (rail_mv > UINT16_MAX) ? UINT16_MAX : (uint16_t)rail_mv;
}
